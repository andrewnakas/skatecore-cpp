// skatecore web demo: the C++ skate library (WebAssembly) on a three.js skatepark.
import * as THREE from 'three';
import SkateCore from './skatecore.js';
import { buildPark } from './park.js';

const BUTTON = { PUSH: 1, BRAKE: 2, GRAB_L: 4, GRAB_R: 8, MANUAL: 16, NOSE: 32, SLIDE: 64 };
const STATE = ['Rolling', 'Manual', 'Powerslide', 'Air', 'Grind', 'Bail'];
const $ = (id) => document.getElementById(id);

const core = await SkateCore();
const park = buildPark();

// ---------- the library's world
function upload(array, heap) {
  const ptr = core._malloc(array.byteLength);
  heap.set(array, ptr >> 2);
  return ptr;
}
const vptr = upload(park.vertices, core.HEAPF32);
const iptr = upload(park.indices, core.HEAP32);
const railCount = core._demo_set_world(vptr, park.vertices.length / 3, iptr, park.indices.length / 3);
core._free(vptr);
core._free(iptr);
const spawn = () => core._demo_spawn(...park.spawn, park.heading);
spawn();
console.log(`skatecore: ${park.indices.length / 3} triangles, ${railCount} grind rails derived`);

// ---------- scene
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;
renderer.toneMapping = THREE.ACESFilmicToneMapping;
$('stage').appendChild(renderer.domElement);

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x0e1116);
scene.fog = new THREE.Fog(0x0e1116, 35, 90);
const camera = new THREE.PerspectiveCamera(62, 1, 0.05, 200);

scene.add(new THREE.HemisphereLight(0xbfd6ff, 0x20242b, 0.9));
const sun = new THREE.DirectionalLight(0xfff1dc, 2.2);
sun.position.set(18, 30, -12);
sun.castShadow = true;
sun.shadow.mapSize.set(2048, 2048);
Object.assign(sun.shadow.camera, { left: -30, right: 30, top: 30, bottom: -30, near: 1, far: 90 });
scene.add(sun, sun.target);

// ground grid texture
function gridTexture() {
  const c = document.createElement('canvas');
  c.width = c.height = 256;
  const g = c.getContext('2d');
  g.fillStyle = '#2a2f36';
  g.fillRect(0, 0, 256, 256);
  g.strokeStyle = 'rgba(255,255,255,0.07)';
  g.lineWidth = 2;
  g.strokeRect(0, 0, 256, 256);
  for (let i = 0; i < 400; i++) {
    g.fillStyle = `rgba(255,255,255,${Math.random() * 0.035})`;
    g.fillRect(Math.random() * 256, Math.random() * 256, 2, 2);
  }
  const t = new THREE.CanvasTexture(c);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

for (const p of park.pieces) {
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute(p.positions, 3));
  geo.computeVertexNormals();
  let material;
  if (p.name === 'ground') {
    const uv = [];
    for (let i = 0; i < p.positions.length; i += 3) uv.push(p.positions[i] / 4, p.positions[i + 2] / 4);
    geo.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
    material = new THREE.MeshStandardMaterial({ map: gridTexture(), roughness: 0.95 });
  } else {
    material = new THREE.MeshStandardMaterial({ color: p.color, roughness: p.name.includes('rail') || p.name === 'flatbar' ? 0.35 : 0.8,
      metalness: p.name.includes('rail') || p.name === 'flatbar' ? 0.6 : 0.0, flatShading: true, side: THREE.DoubleSide });
  }
  const mesh = new THREE.Mesh(geo, material);
  mesh.castShadow = p.name !== 'ground';
  mesh.receiveShadow = true;
  scene.add(mesh);
}

// derived grind rails: a thin bright edge so you can see what's grindable
{
  const lenPtr = core._malloc(4);
  const mat = new THREE.MeshBasicMaterial({ color: 0x6fe3ff, transparent: true, opacity: 0.55 });
  for (let r = 0; r < railCount; r++) {
    const ptr = core._demo_rail_points(r, lenPtr);
    const n = core.HEAP32[lenPtr >> 2];
    const pts = [];
    for (let i = 0; i < n; i++) pts.push(new THREE.Vector3(...core.HEAPF32.subarray((ptr >> 2) + i * 3, (ptr >> 2) + i * 3 + 3)).add(new THREE.Vector3(0, 0.012, 0)));
    for (let i = 0; i + 1 < pts.length; i++) {
      const len = pts[i].distanceTo(pts[i + 1]);
      const m = new THREE.Mesh(new THREE.CylinderGeometry(0.012, 0.012, len, 6), mat);
      m.position.copy(pts[i]).lerp(pts[i + 1], 0.5);
      m.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), pts[i + 1].clone().sub(pts[i]).normalize());
      scene.add(m);
    }
  }
  core._free(lenPtr);
}

// ---------- the board (nose at +Z, contact point at the origin)
const board = new THREE.Group();
{
  const deckMat = new THREE.MeshStandardMaterial({ color: 0x1c1d21, roughness: 0.9 });
  const plyMat = new THREE.MeshStandardMaterial({ color: 0xe0563b, roughness: 0.6 });
  const metal = new THREE.MeshStandardMaterial({ color: 0xb8bec6, metalness: 0.8, roughness: 0.3 });
  const urethane = new THREE.MeshStandardMaterial({ color: 0xf1e7cf, roughness: 0.5 });
  const deck = new THREE.Mesh(new THREE.BoxGeometry(0.21, 0.014, 0.56), [plyMat, plyMat, deckMat, plyMat, plyMat, plyMat]);
  deck.position.y = 0.095;
  board.add(deck);
  for (const end of [-1, 1]) {
    const kick = new THREE.Mesh(new THREE.BoxGeometry(0.2, 0.014, 0.15), [plyMat, plyMat, deckMat, plyMat, plyMat, plyMat]);
    kick.position.set(0, 0.112, end * 0.345);
    kick.rotation.x = -end * 0.32;
    board.add(kick);
    const truck = new THREE.Mesh(new THREE.BoxGeometry(0.16, 0.04, 0.035), metal);
    truck.position.set(0, 0.066, end * 0.21);
    board.add(truck);
    for (const side of [-1, 1]) {
      const wheel = new THREE.Mesh(new THREE.CylinderGeometry(0.027, 0.027, 0.032, 14), urethane);
      wheel.rotation.z = Math.PI / 2;
      wheel.position.set(side * 0.094, 0.027, end * 0.21);
      board.add(wheel);
    }
  }
  board.traverse((m) => { m.castShadow = true; });
  scene.add(board);
}

// ---------- the skater: a simple figure standing sideways on the deck
const rider = new THREE.Group();
const legs = new THREE.Group();
const torso = new THREE.Group();
{
  const skin = new THREE.MeshStandardMaterial({ color: 0xd9a77f, roughness: 0.8 });
  const shirt = new THREE.MeshStandardMaterial({ color: 0x3f8fd6, roughness: 0.85 });
  const pants = new THREE.MeshStandardMaterial({ color: 0x2c3138, roughness: 0.9 });
  const shoes = new THREE.MeshStandardMaterial({ color: 0xeeeeee, roughness: 0.7 });
  for (const z of [-0.17, 0.17]) {
    const leg = new THREE.Mesh(new THREE.CapsuleGeometry(0.075, 0.62, 4, 10), pants);
    leg.position.set(0, 0.42, z);
    legs.add(leg);
    const shoe = new THREE.Mesh(new THREE.BoxGeometry(0.12, 0.07, 0.27), shoes);
    shoe.position.set(0.02, 0.035, z);
    shoe.rotation.y = Math.PI / 2;
    legs.add(shoe);
  }
  const body = new THREE.Mesh(new THREE.CapsuleGeometry(0.17, 0.42, 4, 12), shirt);
  body.position.y = 0.32;
  torso.add(body);
  const head = new THREE.Mesh(new THREE.SphereGeometry(0.115, 16, 12), skin);
  head.position.y = 0.76;
  torso.add(head);
  for (const z of [-0.26, 0.26]) {
    const arm = new THREE.Mesh(new THREE.CapsuleGeometry(0.05, 0.5, 4, 8), shirt);
    arm.position.set(0, 0.32, z);
    arm.rotation.x = z > 0 ? 0.5 : -0.5;
    torso.add(arm);
  }
  torso.position.y = 0.82;
  rider.add(legs, torso);
  rider.traverse((m) => { m.castShadow = true; });
  scene.add(rider);
}

// ---------- input
const keys = new Set();
addEventListener('keydown', (e) => {
  if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Space'].includes(e.code)) e.preventDefault();
  keys.add(e.code);
  if (e.code === 'KeyR') spawn();
  if (e.code === 'KeyH') $('help').classList.toggle('hidden');
});
addEventListener('keyup', (e) => keys.delete(e.code));
addEventListener('blur', () => keys.clear());

const touch = { left: null, right: null };
function stickFrom(t, origin) {
  const dx = (t.clientX - origin.x) / 60, dy = (t.clientY - origin.y) / 60;
  const l = Math.hypot(dx, dy);
  const s = l > 1 ? 1 / l : 1;
  return [dx * s, -dy * s];
}
const stage = $('stage');
stage.addEventListener('touchstart', (e) => {
  e.preventDefault();
  for (const t of e.changedTouches) {
    const side = t.clientX < innerWidth / 2 ? 'left' : 'right';
    touch[side] = { id: t.identifier, x: t.clientX, y: t.clientY, v: [0, 0] };
  }
  $('help').classList.add('hidden');
}, { passive: false });
stage.addEventListener('touchmove', (e) => {
  e.preventDefault();
  for (const t of e.changedTouches) for (const side of ['left', 'right']) if (touch[side]?.id === t.identifier) touch[side].v = stickFrom(t, touch[side]);
}, { passive: false });
const endTouch = (e) => { for (const t of e.changedTouches) for (const side of ['left', 'right']) if (touch[side]?.id === t.identifier) touch[side] = null; };
stage.addEventListener('touchend', endTouch);
stage.addEventListener('touchcancel', endTouch);

function readInput() {
  let lx = (keys.has('KeyD') ? 1 : 0) - (keys.has('KeyA') ? 1 : 0);
  let ly = (keys.has('KeyW') ? 1 : 0) - (keys.has('KeyS') ? 1 : 0);
  let rx = (keys.has('ArrowRight') ? 1 : 0) - (keys.has('ArrowLeft') ? 1 : 0);
  let ry = (keys.has('ArrowUp') ? 1 : 0) - (keys.has('ArrowDown') ? 1 : 0);
  if (rx && ry) { rx *= Math.SQRT1_2; ry *= Math.SQRT1_2; }
  let b = 0;
  if (keys.has('Space') || keys.has('KeyW')) b |= BUTTON.PUSH;
  if (keys.has('KeyS')) b |= BUTTON.BRAKE;
  if (keys.has('KeyQ')) b |= BUTTON.GRAB_L;
  if (keys.has('KeyE')) b |= BUTTON.GRAB_R;
  if (keys.has('ShiftLeft') || keys.has('ShiftRight')) b |= BUTTON.MANUAL;
  if (keys.has('KeyC')) b |= BUTTON.NOSE;
  if (keys.has('KeyX')) b |= BUTTON.SLIDE;
  for (const pad of navigator.getGamepads?.() ?? []) {
    if (!pad) continue;
    const ax = (i) => (Math.abs(pad.axes[i] ?? 0) > 0.15 ? pad.axes[i] : 0);
    const pressed = (i) => pad.buttons[i]?.pressed;
    if (ax(0) || ax(1)) { lx = ax(0); ly = -ax(1); }
    if (ax(2) || ax(3)) { rx = ax(2); ry = -ax(3); }
    if (pressed(0)) b |= BUTTON.PUSH;
    if (pressed(1)) b |= BUTTON.BRAKE;
    if (pressed(2)) b |= BUTTON.SLIDE;
    if (pressed(4)) b |= BUTTON.MANUAL;
    if (pressed(5)) b |= BUTTON.NOSE;
    if (pressed(6)) b |= BUTTON.GRAB_L;
    if (pressed(7)) b |= BUTTON.GRAB_R;
    if (pressed(3) && !pad._respawned) { spawn(); }
    pad._respawned = pressed(3);
  }
  if (touch.left) { [lx, ly] = touch.left.v; if (ly > 0.5) b |= BUTTON.PUSH; if (ly < -0.5) b |= BUTTON.BRAKE; }
  if (touch.right) [rx, ry] = touch.right.v;
  return [lx, ly, rx, ry, b];
}

// ---------- HUD
const fmt = (n) => Math.round(n).toLocaleString('en-US');
const pretty = (n) => n.replace(/^N_/, 'Nollie ').replace(/FSPopShuvit/, 'FS Pop Shove-it').replace(/PopShuvit/, 'Pop Shove-it').replace(/([a-z])([A-Z])/g, '$1 $2');
let trickTimer = 0;
let line = [];
function flash(text, cls = '') {
  const el = $('trick');
  el.textContent = text;
  el.className = 'trick show ' + cls;
  clearTimeout(trickTimer);
  trickTimer = setTimeout(() => (el.className = 'trick'), 1300);
}
function events() {
  for (let type; (type = core._demo_poll_event()); ) {
    const name = core.UTF8ToString(core._demo_event_name());
    const points = core._demo_event_points();
    switch (type - 1) {
      case 0: line.push(pretty(name)); if (line.length > 6) line.shift(); flash(pretty(name)); console.log(`skatecore: trick ${name} ${points}`); break;
      case 2: flash('BAILED', 'bail'); line = []; console.log(`skatecore: bail ${name}`); break;
      case 3: console.log(`skatecore: grind ${name}`); break;
      case 4: flash(`+${fmt(points)}`, 'bank'); line = []; console.log(`skatecore: banked ${points}`); break;
      case 5: console.log(`skatecore: pop ${name}`); break;
    }
    $('line').textContent = line.join(' + ');
  }
}

// ---------- loop: fixed 60 Hz steps, render every frame
const pose = new Float32Array(19);
const posePtr = core._malloc(19 * 4);
const camTarget = new THREE.Vector3();
const camPos = new THREE.Vector3(0, 2, -12);
let accumulator = 0;
let last = performance.now();
let bailSpin = 0;

function resize() {
  renderer.setSize(innerWidth, innerHeight);
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
}
addEventListener('resize', resize);
resize();

function frame(now) {
  accumulator += Math.min((now - last) / 1000, 0.1);
  last = now;
  const input = readInput();
  let steps = 0;
  while (accumulator >= 1 / 60 && steps < 6) {
    core._demo_step(...input);
    accumulator -= 1 / 60;
    steps++;
  }
  events();
  core._demo_pose(posePtr);
  pose.set(core.HEAPF32.subarray(posePtr >> 2, (posePtr >> 2) + 19));

  const [x, y, z, qx, qy, qz, qw, heading, travel, crouch, lean, state] = pose;
  board.position.set(x, y, z);
  board.quaternion.set(qx, qy, qz, qw);

  // rider: sideways on the deck, crouching, leaning into carves; tumbling in a bail
  const bailing = state === 5;
  bailSpin = bailing ? bailSpin + 0.12 : 0;
  rider.position.set(x, y + 0.11, z);
  // feet along the board (local +Z = nose), chest facing the toe side
  rider.quaternion.setFromEuler(new THREE.Euler(0, heading, 0));
  rider.rotateZ(lean * 0.25);
  if (bailing) rider.rotateX(Math.min(bailSpin, Math.PI / 2));
  const squat = Math.min(crouch, 1) * 0.28 + (state === 3 ? 0.12 : 0);
  legs.scale.y = 1 - squat * 0.55;
  torso.position.y = 0.82 - squat * 0.34;
  torso.rotation.z = squat * 0.4;

  // chase camera behind the direction of travel
  const speed = Math.hypot(pose[16], pose[18]);
  const dir = speed > 0.5 ? travel : heading;
  camTarget.lerp(new THREE.Vector3(x, y + 1.0, z), 0.25);
  const want = new THREE.Vector3(x - Math.sin(dir) * 4.2, y + 1.9, z - Math.cos(dir) * 4.2);
  camPos.lerp(want, 0.08);
  camera.position.copy(camPos);
  camera.lookAt(camTarget);
  sun.position.set(x + 18, y + 30, z - 12);
  sun.target.position.set(x, y, z);

  $('score').textContent = fmt(pose[15]);
  $('combo').textContent = pose[13] > 0 ? `${fmt(pose[13])} × ${pose[14]}` : '';
  $('speed').textContent = `${Math.round(speed * 3.6)} km/h`;
  $('state').textContent = STATE[state] ?? '';
  const current = core.UTF8ToString(core._demo_current_trick());
  if (current && (state === 1 || state === 4)) { $('trick').textContent = pretty(current); $('trick').className = 'trick show'; }

  renderer.render(scene, camera);
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
window.skatecore = { core, pose: () => Array.from(pose) };
$('loading').remove();
