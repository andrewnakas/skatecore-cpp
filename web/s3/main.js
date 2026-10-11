// skatecore web demo (stage W2): the full Skate 3 runtime port (C++ -> WebAssembly) skating on the
// cleanroom data. The wasm module reads the pack and a .skate map, runs s3::game::driver::Sim (the
// game's fixed-tick simulation, bit-identical to the Rust reference) and hands back the skater pose and
// the gameplay camera; three.js draws them the way skate-game does (presentation.rs / animation.rs /
// camera.rs), with the web build's input mapping (input/platform.rs) and its clean-audio mixer
// (player_audio_web.rs). The pack is fetched at run time from the cleanroom site and never bundled.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import Skate3 from './skate3.js';

const $ = (id) => document.getElementById(id);
const params = new URLSearchParams(location.search);
const DEBUG = params.has('debug');
// Same-origin path on GitHub Pages; the absolute URL elsewhere (Pages sends Access-Control-Allow-Origin: *).
// ?base=URL/ points at another copy of the cleanroom pack (local testing).
const BASE = params.get('base') || (location.hostname === 'andrewnakas.github.io'
  ? '/skate3-cleanroom/'
  : 'https://andrewnakas.github.io/skate3-cleanroom/');
const ASSET_ROOT = 'assets';
const SKATER_GLB = 'assets/private/skater.glb';
const AUDIO_DIR = 'assets/clean-audio/';

function status(text) { $('status').textContent = text; console.log('s3:', text); }
function fatal(e) {
  console.error(e);
  $('status').textContent = String(e && e.message || e);
  $('status').className = 'error';
  $('loading').classList.remove('done');
}

// ---------- download with progress
const progress = { done: 0, total: 1 };
function showProgress() {
  const f = Math.min(1, progress.done / progress.total);
  $('progress').style.width = `${(f * 100).toFixed(1)}%`;
  $('bytes').textContent = `${(progress.done / 1048576).toFixed(1)} / ${(progress.total / 1048576).toFixed(1)} MB`;
}
// Stream `parts` (relative to BASE) in order, handing every chunk to sink(chunk, offset).
async function streamParts(parts, sink) {
  let offset = 0;
  for (const part of parts) {
    const res = await fetch(BASE + part);
    if (!res.ok) throw new Error(`${part}: HTTP ${res.status}`);
    const reader = res.body.getReader();
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      sink(value, offset);
      offset += value.length;
      progress.done += value.length;
      showProgress();
    }
  }
  return offset;
}

// ---------- wasm helpers
let core;
function withCString(s, fn) {
  const n = core.lengthBytesUTF8(s) + 1;
  const p = core._malloc(n);
  core.stringToUTF8(s, p, n);
  try { return fn(p); } finally { core._free(p); }
}
const str = (p) => core.UTF8ToString(p);
const f32 = (ptr, len) => core.HEAPF32.slice(ptr >> 2, (ptr >> 2) + len);
const u32 = (ptr, len) => core.HEAPU32.slice(ptr >> 2, (ptr >> 2) + len);
function check(ok, what) { if (!ok) throw new Error(`${what}: ${str(core._s3_error())}`); return ok; }
// Copy a VFS file out of the wasm heap.
function readFile(path) {
  const sizePtr = core._malloc(4);
  try {
    const ptr = withCString(path, (p) => core._s3_read_file(p, sizePtr));
    check(ptr, path);
    const size = core.HEAPU32[sizePtr >> 2];
    return core.HEAPU8.slice(ptr, ptr + size);
  } finally { core._free(sizePtr); }
}

// ---------- load everything
async function load() {
  status('loading engine…');
  const [module, manifest] = await Promise.all([
    Skate3(),
    fetch(BASE + 'pack.json').then((r) => { if (!r.ok) throw new Error(`pack.json: HTTP ${r.status}`); return r.json(); }),
  ]);
  core = module;
  // The cleanroom build opens on pack.json's "default" (StartPark), but its authored spawn falls through the
  // floor in this runtime (and in the Rust reference simulation it matches), so open on BlackBoxPark (authored spawn, well lit) instead.
  const DEFAULT_MAP = 'BlackBoxPark';
  const find = (name) => name && manifest.maps.find((m) => m.name.toLowerCase() === String(name).toLowerCase());
  const map = find(params.get('map')) || find(DEFAULT_MAP) || find(manifest.default) || manifest.maps[0];
  const picker = $('maps');
  for (const m of manifest.maps) {
    const o = document.createElement('option');
    o.value = m.name;
    o.textContent = `${m.name} (${Math.max(1, Math.round(m.size / 1048576))} MB)`;
    o.selected = m === map;
    picker.appendChild(o);
  }
  picker.addEventListener('change', () => {
    const q = new URLSearchParams(location.search);
    q.set('map', picker.value);
    location.search = q.toString();
  });

  progress.total = manifest.core.size + map.size;
  status(`downloading core.pack + ${map.name}…`);
  // The pack streams straight into a wasm buffer; the map is small enough to buffer in JS meanwhile.
  const packPtr = core._s3_buffer(manifest.core.size);
  check(packPtr, 'pack buffer');
  const mapBytes = new Uint8Array(map.size);
  const [packSize, mapSize] = await Promise.all([
    streamParts(manifest.core.parts, (chunk, off) => {
      if (off + chunk.length > manifest.core.size) throw new Error('core.pack is larger than pack.json says');
      core.HEAPU8.set(chunk, packPtr + off);
    }),
    streamParts(map.parts, (chunk, off) => {
      if (off + chunk.length > map.size) throw new Error(`${map.name} is larger than pack.json says`);
      mapBytes.set(chunk, off);
    }),
  ]);
  if (packSize !== manifest.core.size) throw new Error(`core.pack: got ${packSize} of ${manifest.core.size} bytes`);
  if (mapSize !== map.size) throw new Error(`${map.name}: got ${mapSize} of ${map.size} bytes`);

  status('installing pack…');
  await new Promise(requestAnimationFrame);
  const files = check(core._s3_load_pack(), 'core.pack');
  const mapPath = `maps/${map.name}.skate`;
  const mapPtr = core._s3_buffer(map.size);
  core.HEAPU8.set(mapBytes, mapPtr);
  withCString(mapPath, (p) => check(core._s3_add_file(p), mapPath));
  status(`decoding ${map.name}…`);
  await new Promise(requestAnimationFrame);
  const t0 = performance.now();
  withCString(mapPath, (p) => check(core._s3_map_load(p), mapPath));
  console.log(`s3: pack ${files} files; ${map.name} decoded in ${(performance.now() - t0).toFixed(0)} ms`);
  return { map, mapPath, manifest };
}

// ---------- the map
function buildMap() {
  const version = core._s3_map_version();
  const nv = core._s3_map_vertex_count();
  const ni = core._s3_map_index_count();
  const geom = new THREE.BufferGeometry();
  geom.setAttribute('position', new THREE.BufferAttribute(f32(core._s3_map_positions(), nv * 3), 3));
  geom.setAttribute('normal', new THREE.BufferAttribute(f32(core._s3_map_normals(), nv * 3), 3));
  geom.setAttribute('uv', new THREE.BufferAttribute(f32(core._s3_map_uvs(), nv * 2), 2));
  geom.setAttribute('uv1', new THREE.BufferAttribute(f32(core._s3_map_uv1s(), nv * 2), 2));
  geom.setIndex(new THREE.BufferAttribute(u32(core._s3_map_indices(), ni), 1));

  // Textures, decoded by the loader; roles follow skate_world.rs (0 albedo/emissive, 1 lightmap, 2 data).
  const ntex = core._s3_map_texture_count();
  const texCache = new Map();
  function texture(id, role) {
    if (!id || id > ntex) return null;
    const key = `${id}/${role}`;
    if (texCache.has(key)) return texCache.get(key);
    const i = id - 1;
    const w = core._s3_map_texture_info(i, 0), h = core._s3_map_texture_info(i, 1), cs = core._s3_map_texture_info(i, 2);
    const ptr = core._s3_map_texture_rgba(i);
    const rgba = core.HEAPU8.subarray(ptr, ptr + w * h * 4);
    let tex;
    if (role === 1) {
      // Baked lighting: v * v * 4 per colour channel (skate_world.rs), kept as half floats.
      const half = new Uint16Array(w * h * 4);
      for (let k = 0; k < half.length; k++) {
        const v = rgba[k] / 255;
        half[k] = THREE.DataUtils.toHalfFloat((k & 3) === 3 ? 1 : v * v * 4);
      }
      tex = new THREE.DataTexture(half, w, h, THREE.RGBAFormat, THREE.HalfFloatType);
      tex.colorSpace = THREE.LinearSRGBColorSpace;
      tex.channel = 1;
    } else {
      tex = new THREE.DataTexture(rgba.slice(), w, h, THREE.RGBAFormat, THREE.UnsignedByteType);
      const srgb = role === 0 && cs === 1 && !(version >= 2 && version <= 3);
      tex.colorSpace = srgb ? THREE.SRGBColorSpace : THREE.NoColorSpace;
      tex.wrapS = tex.wrapT = THREE.RepeatWrapping;
      tex.generateMipmaps = true;
      tex.minFilter = THREE.LinearMipmapLinearFilter;
      tex.anisotropy = 8;
    }
    tex.magFilter = THREE.LinearFilter;
    tex.name = str(core._s3_map_texture_name(i));
    tex.needsUpdate = true;
    texCache.set(key, tex);
    return tex;
  }

  const stride = core._s3_map_material_stride();
  const nm = core._s3_map_material_count();
  const rows = f32(core._s3_map_materials(), nm * stride);
  const materials = [];
  const matIndex = new Map();
  function material(id) {
    if (matIndex.has(id)) return matIndex.get(id);
    const r = rows.subarray(id * stride, (id + 1) * stride);
    const [, , , cr, cg, cb, rough, emissive, tAlbedo, tIndirect, tNormal, tOrm, tEmissive, alphaMode, cutoff, indirect] = r;
    const orm = texture(tOrm, 2);
    const m = new THREE.MeshStandardMaterial({
      name: str(core._s3_map_material_name(id)),
      color: new THREE.Color().setRGB(cr, cg, cb),
      map: texture(tAlbedo, 0),
      normalMap: texture(tNormal, 2),
      roughnessMap: orm, metalnessMap: orm, aoMap: orm,
      metalness: tOrm ? 1 : 0,
      roughness: rough,
      emissive: new THREE.Color().setRGB(cr * emissive, cg * emissive, cb * emissive),
      emissiveMap: texture(tEmissive, 0),
    });
    if (alphaMode === 1) m.alphaTest = cutoff;
    else if (alphaMode === 2) { m.transparent = true; m.depthWrite = false; }
    const lm = texture(tIndirect, 1);
    if (lm) { m.lightMap = lm; m.lightMapIntensity = indirect * Math.PI; }
    const index = materials.length;
    materials.push(m);
    matIndex.set(id, index);
    return index;
  }
  const ng = core._s3_map_group_count();
  const groups = u32(core._s3_map_groups(), ng * 3);
  for (let g = 0; g < ng; g++) {
    const [id, start, count] = groups.subarray(g * 3, g * 3 + 3);
    geom.addGroup(start, count, material(Math.min(id, nm - 1)));
  }
  geom.computeBoundingSphere();
  geom.computeBoundingBox();
  const mesh = new THREE.Mesh(geom, materials);
  mesh.name = str(core._s3_map_name());
  mesh.castShadow = true;
  mesh.receiveShadow = true;

  const sp = f32(core._s3_map_spawn(), 4);
  const env = f32(core._s3_map_environment(), core._s3_map_environment_count());
  const ls = core._s3_map_light_stride();
  const lights = f32(core._s3_map_lights(), core._s3_map_light_count() * ls);
  console.log(`s3: map ${mesh.name} v${version}: ${nv} vertices, ${ni / 3} triangles, ${ng} batches, ${nm} materials, ` +
    `${ntex} textures, ${core._s3_map_rail_count()} rails, ${core._s3_map_collision_count()} collision tris, ` +
    `${lights.length / ls} lights; spawn ${[...sp.subarray(0, 3)].map((v) => v.toFixed(2))} heading ${sp[3].toFixed(3)}`);
  return { mesh, spawn: new THREE.Vector3(sp[0], sp[1], sp[2]), heading: sp[3], env, lights, ls };
}

// Custom-map daylight (map_render.rs daylight_values). Bevy units: lux / cd·m⁻² at the default
// camera exposure (EV100 9.7, a factor of 1 / (1.2 · 2^9.7)); three.js lights are unit-free.
const BEVY_EXPOSURE = 1 / (1.2 * 2 ** 9.7);
function daylight(e, hour) {
  const angle = (hour - 6) * Math.PI / 12;
  const elevation = Math.sin(angle);
  const day = Math.max(elevation, 0);
  const extended = e.length === 45;
  const night = elevation < 0;
  const rgb = (i) => new THREE.Color().setRGB(Math.max(e[i], 0), Math.max(e[i + 1], 0), Math.max(e[i + 2], 0));
  const color = extended ? rgb(night ? 35 : 32)
    : night ? new THREE.Color().setRGB(0.55, 0.65, 1, THREE.SRGBColorSpace) : new THREE.Color(1, 1, 1);
  const strength = extended ? Math.max(e[night ? 39 : 38], 0) : night ? 0.025 : 1;
  const ambient = extended ? e[41] + (e[40] - e[41]) * day : 0.025 + 0.295 * day;
  const horizontal = Math.cos(angle) * (night ? -1 : 1);
  const direction = new THREE.Vector3(Math.cos(e[11]) * horizontal, Math.max(Math.abs(elevation), 0.001),
    Math.sin(e[11]) * horizontal).normalize();
  return {
    color, direction, day,
    illuminance: 10000 * strength * Math.abs(elevation),
    ambient: Math.max(ambient, 0) * 1000,
  };
}

// ---------- scene
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;
renderer.toneMapping = THREE.AgXToneMapping; // Bevy's default TonyMcMapface has no three.js twin
renderer.toneMappingExposure = 1;
$('stage').appendChild(renderer.domElement);
const scene = new THREE.Scene();
// Bevy's perspective defaults: near 0.1 (reversed-Z, infinite far).
const camera = new THREE.PerspectiveCamera(60, innerWidth / innerHeight, 0.1, 4000);
const orbit = new OrbitControls(camera, renderer.domElement);
orbit.enableDamping = true;
orbit.enabled = false;
let simReady = false;
addEventListener('resize', () => {
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
  // camera.rs present: the game camera's lens follows the window aspect ratio.
  if (simReady) core._s3_sim_set_aspect(innerWidth / innerHeight);
});

let sun;
function light(world) {
  const e = world.env;
  if (e.length >= 6) scene.background = new THREE.Color().setRGB(e[3], e[4], e[5]);
  const hour = e.length > 10 ? ((e[10] % 24) + 24) % 24 : 12;
  const d = e.length > 11 ? daylight(e, hour) : daylight(new Float32Array(12), 12);
  // Ambient: Bevy adds brightness · exposure · albedo; three.js divides ambient by π.
  scene.add(new THREE.AmbientLight(0xffffff, d.ambient * BEVY_EXPOSURE * Math.PI));
  sun = new THREE.DirectionalLight(d.color, d.illuminance * BEVY_EXPOSURE);
  sun.userData.direction = d.direction.clone();
  sun.position.copy(world.spawn).addScaledVector(d.direction, 80);
  sun.target.position.copy(world.spawn);
  sun.castShadow = true;
  sun.shadow.mapSize.set(2048, 2048);
  const s = sun.shadow.camera;
  s.left = s.bottom = -30; s.right = s.top = 30; s.near = 1; s.far = 200;
  sun.shadow.bias = -0.0005;
  sun.shadow.normalBias = 0.02;
  scene.add(sun, sun.target);
  // Authored point (0) and spot (1) lights: Bevy lumens -> candela (/4π) at the same exposure.
  for (let o = 0; o < world.lights.length; o += world.ls) {
    const [kind, px, py, pz, dx, dy, dz, r, g, b, intensity, range, , innerCos, outerCos] = world.lights.subarray(o, o + world.ls);
    const color = new THREE.Color().setRGB(r, g, b);
    const cd = intensity / (4 * Math.PI) * BEVY_EXPOSURE;
    let l;
    if (kind === 0) l = new THREE.PointLight(color, cd, range, 2);
    else if (kind === 1) {
      l = new THREE.SpotLight(color, cd, range, Math.acos(outerCos), 1 - Math.acos(innerCos) / Math.max(Math.acos(outerCos), 1e-4), 2);
      l.target.position.set(px + dx, py + dy, pz + dz);
      scene.add(l.target);
    } else continue;
    l.position.set(px, py, pz);
    scene.add(l);
  }
}

// ---------- the skater: skater.glb skinned by the simulation's render pose (animation.rs)
// The GLB went through Blender: its bone-local axes are rotated -90° about X relative to the native
// frames. animation.rs render_basis = Mat4::from_cols(X, -Z, Y, W).
const BASIS = new THREE.Matrix4().set(
  1, 0, 0, 0,
  0, 0, 1, 0,
  0, -1, 0, 0,
  0, 0, 0, 1);
async function buildSkater() {
  const bytes = readFile(SKATER_GLB);
  const gltf = await new Promise((resolve, reject) => new GLTFLoader().parse(bytes.buffer, '', resolve, reject));
  const root = gltf.scene;
  const skinned = [];
  root.traverse((o) => { if (o.isSkinnedMesh) skinned.push(o); });
  // Rig bone names (the stock ABIN hierarchy = the evaluator's bone order).
  const n = withCString(ASSET_ROOT, (p) => core._s3_rig_load(p));
  check(n, 'rig');
  const names = [];
  for (let i = 0; i < n; i++) names.push(str(core._s3_rig_bone_name(i)).toUpperCase());
  // Bind every skin joint by name, with the nearest ancestor joint as its parent bone (animation.rs bind).
  const joints = [];
  const seen = new Set();
  for (const mesh of skinned) {
    mesh.castShadow = true;
    mesh.receiveShadow = true;
    mesh.frustumCulled = false; // GLB bounds describe the bind pose
    for (const bone of mesh.skeleton.bones) {
      if (seen.has(bone)) continue;
      seen.add(bone);
      const index = names.indexOf(bone.name.toUpperCase());
      if (index < 0) throw new Error(`skater skin bone ${bone.name} is absent from the stock rig`);
      joints.push({ bone, index });
    }
  }
  const byBone = new Map(joints.map((j) => [j.bone, j]));
  for (const j of joints) {
    j.parent = null;
    for (let p = j.bone.parent; p && p !== root; p = p.parent) {
      if (byBone.has(p)) { j.parent = byBone.get(p); break; }
    }
  }
  const depth = (j) => (j.parent ? 1 + depth(j.parent) : 0);
  joints.sort((a, b) => depth(a) - depth(b));
  root.updateMatrixWorld(true);
  for (const j of joints) {
    j.bone.matrixAutoUpdate = false;
    j.bone.matrixWorldAutoUpdate = false;
    j.world = j.bone.matrixWorld;
  }
  scene.add(root);
  console.log(`s3: skater.glb ${(bytes.length / 1048576).toFixed(1)} MB, ${skinned.length} skinned primitives, ` +
    `${joints.length} joints -> rig bones [${joints.map((j) => j.index).join(',')}]`);
  return { root, skinned, joints };
}

// ---------- presentation snapshots (presentation.rs): root, bone locals, camera, fov per completed tick
const _m = new THREE.Matrix4(), _n = new THREE.Matrix4(), _p = new THREE.Matrix4();
function decompose(m) {
  const t = { p: new THREE.Vector3(), q: new THREE.Quaternion(), s: new THREE.Vector3() };
  m.decompose(t.p, t.q, t.s);
  return t;
}
function blend(a, b, alpha, out) {
  out.p.lerpVectors(a.p, b.p, alpha);
  out.q.slerpQuaternions(a.q, b.q, alpha);
  out.s.lerpVectors(a.s, b.s, alpha);
  return out;
}
// animation.rs native_matrix: the native 4th lanes are not part of the transform (w = 0, 0, 0, 1).
function native(m, array, offset) {
  m.fromArray(array, offset);
  const e = m.elements;
  e[3] = 0; e[7] = 0; e[11] = 0; e[15] = 1;
  return m;
}
function capture(skater, view, bones) {
  const root = decompose(native(_m, view, 0));
  // Bone-local transforms relative to the parent bone, both in the skin basis (animation.rs present).
  const locals = skater.joints.map((j) => {
    const global = native(_m, bones, j.index * 16).multiply(BASIS);
    if (j.parent) {
      native(_n, bones, j.parent.index * 16).multiply(BASIS).invert();
      return decompose(_p.multiplyMatrices(_n, global));
    }
    return decompose(global);
  });
  // camera.rs camera_transform: rotation from (-right, up, -at); native At is forward.
  const right = new THREE.Vector3(view[19], view[20], view[21]);
  const up = new THREE.Vector3(view[22], view[23], view[24]);
  const at = new THREE.Vector3(view[25], view[26], view[27]);
  const cam = {
    p: new THREE.Vector3(view[16], view[17], view[18]),
    q: new THREE.Quaternion().setFromRotationMatrix(_m.makeBasis(right.negate(), up, at.negate())).normalize(),
    s: new THREE.Vector3(1, 1, 1),
  };
  return { root, locals, cam, fov: view[28], hasCamera: view[29] > 0 };
}
const _t = { p: new THREE.Vector3(), q: new THREE.Quaternion(), s: new THREE.Vector3() };
const _rootM = new THREE.Matrix4(), _localM = new THREE.Matrix4();
function present(skater, prev, cur, alpha) {
  blend(prev.root, cur.root, alpha, _t);
  _rootM.compose(_t.p, _t.q, _t.s);
  for (let i = 0; i < skater.joints.length; i++) {
    const j = skater.joints[i];
    blend(prev.locals[i], cur.locals[i], alpha, _t);
    _localM.compose(_t.p, _t.q, _t.s);
    j.world.multiplyMatrices(j.parent ? j.parent.world : _rootM, _localM);
  }
  if (!orbit.enabled && cur.hasCamera) {
    blend(prev.cam, cur.cam, alpha, _t);
    camera.position.copy(_t.p);
    camera.quaternion.copy(_t.q);
    const fov = prev.fov + (cur.fov - prev.fov) * alpha;
    if (Math.abs(camera.fov - fov) > 1e-4) { camera.fov = fov; camera.updateProjectionMatrix(); }
  }
  return _rootM;
}

// ---------- input: keyboard + browser gamepads -> the raw XInput packet (input/platform.rs web)
const keys = new Set();
const BUTTONS = [ // [bit, key code, standard-gamepad button]
  [0x0001, 'Digit1', 12], [0x0002, 'Digit2', 13], [0x0004, 'Digit3', 14], [0x0008, 'Digit4', 15],
  [0x0010, 'Enter', 9], [0x0020, 'Backspace', 8], [0x0040, 'KeyV', 10], [0x0080, 'KeyB', 11],
  [0x0100, 'KeyQ', 4], [0x0200, 'KeyR', 5], [0x1000, 'Space', 0], [0x2000, 'KeyE', 1],
  [0x4000, 'ShiftLeft', 2], [0x8000, 'KeyF', 3],
];
const GAME_KEYS = new Set([...BUTTONS.map((b) => b[1]), 'Escape', 'KeyZ', 'KeyC', 'KeyW', 'KeyA', 'KeyS', 'KeyD',
  'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight']);
addEventListener('keydown', (e) => {
  audio.resume();
  if (e.code === 'KeyO' && !e.repeat) { toggleOrbit(); return; }
  if (e.code === 'KeyH' && !e.repeat) { $('help').classList.toggle('hidden'); return; }
  if (GAME_KEYS.has(e.code)) {
    keys.add(e.code);
    e.preventDefault();
    if (params.has('debugkeys')) console.log(`s3: key ${e.code}`);
  }
});
addEventListener('keyup', (e) => { keys.delete(e.code); });
addEventListener('blur', () => keys.clear());
addEventListener('pointerdown', () => audio.resume());
const axis = (v) => Math.trunc(Math.max(-1, Math.min(1, v)) * 32767);
const trigger = (v) => Math.trunc(Math.max(0, Math.min(1, v)) * 255);
const keyAxis = (neg, pos) => (keys.has(pos) ? 1 : 0) - (keys.has(neg) ? 1 : 0);
function keyboardState() {
  let buttons = 0;
  for (const [bit, code] of BUTTONS) if (keys.has(code)) buttons |= bit;
  if (keys.has('Escape')) buttons |= 0x0010;
  return {
    buttons,
    lt: keys.has('KeyZ') ? 255 : 0, rt: keys.has('KeyC') ? 255 : 0,
    lx: axis(keyAxis('KeyA', 'KeyD')), ly: axis(keyAxis('KeyS', 'KeyW')),
    rx: axis(keyAxis('ArrowLeft', 'ArrowRight')), ry: axis(keyAxis('ArrowDown', 'ArrowUp')),
  };
}
function gamepadState(pad) {
  let buttons = 0;
  const pressed = (i) => pad.buttons[i] && pad.buttons[i].pressed;
  for (const [bit, , b] of BUTTONS) if (pressed(b)) buttons |= bit;
  const analog = (i) => (pad.buttons[i] ? pad.buttons[i].value || (pad.buttons[i].pressed ? 1 : 0) : 0);
  const a = (i) => pad.axes[i] || 0;
  // Browser axes are +Y down; XInput (and gilrs) are +Y up.
  return { buttons, lt: trigger(analog(6)), rt: trigger(analog(7)), lx: axis(a(0)), ly: axis(-a(1)), rx: axis(a(2)), ry: axis(-a(3)) };
}
function merge(a, b) {
  const pick = (x, y) => (Math.abs(x) >= Math.abs(y) ? x : y);
  return {
    buttons: a.buttons | b.buttons, lt: Math.max(a.lt, b.lt), rt: Math.max(a.rt, b.rt),
    lx: pick(a.lx, b.lx), ly: pick(a.ly, b.ly), rx: pick(a.rx, b.rx), ry: pick(a.ry, b.ry),
  };
}
function sampleInput() {
  let state = keyboardState();
  const pads = navigator.getGamepads ? [...navigator.getGamepads()].filter(Boolean) : [];
  if (pads.length) state = merge(state, gamepadState(pads[0]));
  return state;
}

// ---------- audio: the clean-room sound set mixed like player_audio_web.rs
const audio = {
  ctx: null, input: null, samples: new Map(), beds: new Map(), edges: null, seed: 0x9E3779B9,
  resume() { if (this.ctx && this.ctx.state !== 'running') this.ctx.resume().catch(() => {}); },
  random() { // xorshift32, as the Rust mixer
    let s = this.seed;
    s = (s ^ (s << 13)) >>> 0;
    s = (s ^ (s >>> 17)) >>> 0;
    s = (s ^ (s << 5)) >>> 0;
    this.seed = s;
    return (s >>> 8) / (1 << 24);
  },
};
const BEDS = ['roll_smooth', 'roll_rough', 'roll_wood', 'roll_metal', 'grind_metal', 'grind_concrete', 'powerslide'];
async function loadAudio() {
  let manifest;
  try { manifest = JSON.parse(new TextDecoder().decode(readFile(AUDIO_DIR + 'sounds.json'))); } catch (e) {
    console.warn('s3: web player audio disabled:', e.message);
    return;
  }
  const Ctx = window.AudioContext || window.webkitAudioContext;
  if (!Ctx) return;
  const ctx = new Ctx();
  // Soft limit (tanh) instead of hard clipping when several cues stack: x -> tanh(x) for |x| <= 4.
  const pre = ctx.createGain();
  pre.gain.value = 0.25;
  const shaper = ctx.createWaveShaper();
  const curve = new Float32Array(2049);
  for (let i = 0; i < curve.length; i++) curve[i] = Math.tanh(((i / (curve.length - 1)) * 2 - 1) * 4);
  shaper.curve = curve;
  pre.connect(shaper).connect(ctx.destination);
  for (const [name, entry] of Object.entries(manifest.sounds || {})) {
    try {
      const bytes = readFile(AUDIO_DIR + entry.file);
      const buffer = await ctx.decodeAudioData(bytes.buffer);
      audio.samples.set(name, { buffer, gain: entry.gain ?? 1 });
    } catch (e) { console.warn(`s3: ${entry.file}: ${e.message}`); }
  }
  for (const name of BEDS) {
    const s = audio.samples.get(name);
    if (!s) continue;
    const src = ctx.createBufferSource();
    src.buffer = s.buffer;
    src.loop = true;
    const gain = ctx.createGain();
    gain.gain.value = 0;
    src.connect(gain).connect(pre);
    src.start();
    audio.beds.set(name, { src, gain, sampleGain: s.gain, targetGain: 0, targetPitch: 1 });
  }
  audio.ctx = ctx;
  audio.input = pre;
  console.log(`s3: web player audio: ${audio.samples.size} clean sounds`);
}
function play(name, gain, pitch) {
  if (!audio.ctx || gain <= 0) return;
  const variants = [...audio.samples.entries()]
    .filter(([k]) => k === name || (k.startsWith(name + '_') && /^\d+$/.test(k.slice(name.length + 1))))
    .map(([, s]) => s);
  if (!variants.length) return;
  const s = variants[Math.min(variants.length - 1, Math.floor(audio.random() * variants.length))];
  const jitter = 1 + (audio.random() - 0.5) * 0.08;
  const src = audio.ctx.createBufferSource();
  src.buffer = s.buffer;
  src.playbackRate.value = pitch * jitter;
  const g = audio.ctx.createGain();
  g.gain.value = gain * s.gain;
  src.connect(g).connect(audio.input);
  src.start();
  if (DEBUG) console.log(`s3: sound ${name}`);
}
const rollBed = (surface) => ([1, 2, 6].includes(surface) ? 'roll_rough' : surface === 5 ? 'roll_wood' : surface === 9 ? 'roll_metal' : 'roll_smooth');
const spins = (id) => ['flip', 'shuv', 'varial', 'heel', 'kick'].some((k) => (id || '').toLowerCase().includes(k));
// One observation per completed tick (physics/audio_observation.rs -> player_audio_web.rs observe).
function observeAudio(o) {
  if (!audio.ctx) return;
  const speed = Math.abs(o.speed);
  const rolling = o.grounded && !o.grinding && !o.wiping && o.contacts > 0;
  const roll = rollBed(o.wheelSurface);
  const level = Math.max(0, Math.min(1, speed / 12));
  const now = audio.ctx.currentTime;
  for (const [name, bed] of audio.beds) {
    let gain = 0, pitch = bed.targetPitch;
    if (name === roll && rolling) { gain = 0.15 + 0.6 * level; pitch = 0.9 + 0.25 * level; }
    else if (name === 'grind_metal' && o.grinding && o.grindSurface === 9) { gain = 0.35 + 0.4 * level; pitch = 0.9 + 0.2 * level; }
    else if (name === 'grind_concrete' && o.grinding && o.grindSurface !== 9) { gain = 0.35 + 0.4 * level; pitch = 0.9 + 0.2 * level; }
    else if (name === 'powerslide' && o.powersliding && o.grounded) { gain = 0.5 * level + 0.1; pitch = 1; }
    const target = speed < 0.2 && name.startsWith('roll') ? 0 : gain;
    if (target !== bed.targetGain || pitch !== bed.targetPitch) {
      // Gains and pitches glide (~60 ms) so a bed never clicks.
      bed.gain.gain.setTargetAtTime(target * bed.sampleGain, now, 0.06);
      bed.src.playbackRate.setTargetAtTime(pitch, now, 0.06);
    }
    bed.targetGain = target;
    bed.targetPitch = pitch;
  }
  const e = audio.edges;
  if (!e) { // the first observation only primes the edges
    audio.edges = { grounded: o.grounded, takeoff: o.tick, pushing: o.pushSpeed > 0, trick: o.trick };
    return;
  }
  if (e.grounded && !o.grounded && !o.wiping) play('pop', 0.8, 1);
  const air = Math.max(0, o.tick - e.takeoff) / 60;
  let landed = false;
  for (const ev of o.events) {
    if (ev === 'LANDING') {
      landed = true;
      // The impact at a fixed level, plus one ladder layer by time in air (this set has neither layer).
      play('land_impact', 1, 1);
      play(air >= 0.75 ? 'land_hard' : 'land_soft', 1, 1);
    } else if (ev === 'WIPEOUT') play('bail', 1, 1);
  }
  if (!landed && !e.grounded && o.grounded && !o.wiping && air >= 0.2) play('land_impact', 1, 1);
  if (o.footstep > 0) play('footstep', Math.max(0.2, Math.min(1, o.footstep)), 1);
  const pushing = o.pushSpeed > 0;
  if (pushing && !e.pushing) play('push', 0.7, 1);
  if (!o.grounded && o.trick && o.trick !== e.trick && spins(o.trick)) play('flip', 0.6, 1);
  if (!o.grounded && spins(e.trick) && o.trick !== e.trick) play('catch', 0.9, 1);
  if (e.grounded && !o.grounded) e.takeoff = o.tick;
  e.grounded = o.grounded;
  e.pushing = pushing;
  e.trick = o.trick;
}

// ---------- HUD (scoring_hud.rs inputs: trick name, sequence score, multiplier, line score)
// The composed name is "#" + space-separated localisation ids and bare numbers (scoring_runtime.rs
// compose_trick_name); apt_text resolves it token by token from the HUD movie's language table.
const hud = { language: {}, name: '', seq: 0, changedAt: -1e9, last: '' };
function loadLanguage() {
  try {
    const movie = JSON.parse(new TextDecoder().decode(readFile('assets/private/hud/runtime/trickdisplay.json')));
    hud.language = movie.language || {};
  } catch (e) { console.warn('s3: HUD language table unavailable:', e.message); }
}
function localise(name) {
  if (!name.startsWith('#')) return name;
  return name.slice(1).split(' ').filter(Boolean)
    .map((t) => (hud.language[t] !== undefined ? hud.language[t] : /^\d+$/.test(t) ? t : t.replace(/^ID_(TRICK_)?/, '').replace(/_/g, ' ')))
    .join(' ');
}
function updateHud(view, now) {
  const name = str(core._s3_sim_score_name());
  const seq = Math.round(view[46]);
  const mult = view[47];
  const line = Math.round(view[48]);
  // The trick display shows the sequence while it scores and lingers briefly after it ends.
  if (seq > 0 && (name !== hud.name || seq !== hud.seq)) hud.changedAt = now;
  hud.name = name;
  hud.seq = seq;
  const on = seq > 0 || now - hud.changedAt < 2500;
  const text = on && name ? localise(name) : '';
  const score = on && seq > 0 ? `${seq.toLocaleString()}${mult > 1 ? `  ×${mult.toFixed(1)}` : ''}` : '';
  const lineText = line > 0 ? `${line.toLocaleString()}` : '';
  const key = `${text}|${score}|${lineText}`;
  if (key !== hud.last) {
    hud.last = key;
    $('trick').textContent = text;
    $('seq').textContent = score;
    $('line').textContent = lineText;
    $('hud').classList.toggle('on', !!(text || score));
    if (text) console.log(`s3: hud "${text}" ${score}`);
  }
}

function toggleOrbit() {
  orbit.enabled = !orbit.enabled;
  if (orbit.enabled) {
    const p = new THREE.Vector3().setFromMatrixPosition(sim.rootMatrix);
    orbit.target.copy(p).add(new THREE.Vector3(0, 0.9, 0));
    orbit.update();
  }
  $('mode').textContent = orbit.enabled ? 'debug orbit camera · O returns to the game camera' : '';
  console.log(`s3: camera ${orbit.enabled ? 'debug orbit' : 'gameplay'}`);
}

// ---------- the simulation loop (Bevy FixedUpdate: accumulate real time, tick at the clock period,
// render interpolated by the overstep fraction)
const sim = { skater: null, prev: null, cur: null, acc: 0, period: 1 / 60, last: 0, failed: false,
  rootMatrix: new THREE.Matrix4(), state: '', ticks: 0, tickMs: 0, view: null };
const MAX_DELTA = 0.25; // Bevy Time<Virtual> max_delta
const LOG_EVENTS = /^(TRICK|SCORE|LANDING|WIPEOUT|AIRBORNE|LANDED|BAIL)/;
function tick(input) {
  const t0 = performance.now();
  if (!core._s3_sim_step(input.buttons, input.lx, input.ly, input.rx, input.ry, input.lt, input.rt)) {
    sim.failed = true;
    const err = str(core._s3_error());
    console.error('s3: simulation stopped:', err);
    $('state').textContent = `simulation stopped: ${err}`;
    return false;
  }
  sim.tickMs += performance.now() - t0;
  sim.ticks++;
  const vp = core._s3_sim_view() >> 2;
  const view = core.HEAPF32.slice(vp, vp + 64);
  const bp = core._s3_sim_bones() >> 2;
  const bones = core.HEAPF32.subarray(bp, bp + core._s3_sim_bone_count() * 16);
  const next = capture(sim.skater, view, bones);
  // Snap teleports/cuts and cadence changes instead of blending across them (presentation.rs capture).
  const reset = view[30] > 0 || view[31] > 0 || Math.abs(view[32] - sim.period) > 1e-9;
  sim.period = view[32] || sim.period;
  sim.prev = !sim.cur || reset ? next : sim.cur;
  sim.cur = next;
  const stateName = str(core._s3_sim_state_name());
  const events = str(core._s3_sim_events()).split('\n').filter(Boolean);
  if (stateName !== sim.state) {
    console.log(`s3: state ${sim.state || '-'} -> ${stateName} (tick ${view[45]}, speed ${view[34].toFixed(2)} m/s)`);
    sim.state = stateName;
  }
  for (const e of events) {
    if (DEBUG ? !e.startsWith('CONTACT') : LOG_EVENTS.test(e)) console.log(`s3: t${view[45]} ${e}`);
  }
  observeAudio({
    tick: view[45], speed: view[34], grounded: view[40] > 0, wiping: view[41] > 0, contacts: view[43],
    grinding: Math.floor(view[44] / 100) === 4, wheelSurface: view[35], grindSurface: view[36], powersliding: view[37] > 0,
    footstep: view[38], pushSpeed: view[39], trick: str(core._s3_sim_trick_identifier()) || null, events,
  });
  sim.view = view;
  return true;
}
const _sunP = new THREE.Vector3();
function frame(now) {
  const dt = Math.min(MAX_DELTA, sim.last ? (now - sim.last) / 1000 : 0);
  sim.last = now;
  if (simReady && !sim.failed) {
    sim.acc += dt;
    const input = sampleInput(); // PreUpdate: one packet per host frame
    let n = 0;
    while (sim.acc >= sim.period && n < 16) {
      sim.acc -= sim.period;
      if (!tick(input)) break;
      n++;
    }
    if (n === 16) sim.acc = 0; // too slow to keep up: drop time rather than spiral
    if (sim.cur) {
      const alpha = Math.max(0, Math.min(1, sim.acc / sim.period));
      sim.rootMatrix.copy(present(sim.skater, sim.prev, sim.cur, alpha));
      // Keep the sun's shadow box on the skater.
      _sunP.setFromMatrixPosition(sim.rootMatrix);
      sun.target.position.copy(_sunP);
      sun.position.copy(_sunP).addScaledVector(sun.userData.direction, 80);
      updateHud(sim.view, now);
      $('state').textContent = `${sim.state} · ${(sim.view[34] * 3.6).toFixed(0)} km/h`;
    }
  }
  if (orbit.enabled) orbit.update();
  renderer.render(scene, camera);
}

// ---------- main
try {
  const { map, mapPath } = await load();
  status('building scene…');
  const world = buildMap();
  scene.add(world.mesh);
  light(world);
  sim.skater = await buildSkater();
  loadLanguage();
  status('loading sounds…');
  await loadAudio();
  status('starting the simulation…');
  await new Promise(requestAnimationFrame);
  const t0 = performance.now();
  withCString(mapPath, (p) => check(core._s3_sim_load(p), 'simulation'));
  core._s3_sim_set_aspect(innerWidth / innerHeight);
  simReady = true;
  sim.period = core._s3_sim_time_step() || 1 / 60;
  console.log(`s3: simulation loaded in ${(performance.now() - t0).toFixed(0)} ms, time step ${sim.period.toFixed(6)} s`);
  tick(sampleInput()); // the first tick publishes the pose and the camera frame
  $('loading').classList.add('done');
  $('info').textContent = `${map.name} · the C++ Skate 3 runtime in WebAssembly`;
  window.s3 = { core, scene, camera, orbit, world, sim, THREE };
  setInterval(() => {
    if (sim.ticks) console.log(`s3: perf ${sim.ticks} ticks, ${(sim.tickMs / sim.ticks).toFixed(2)} ms/tick, state ${sim.state}`);
    sim.ticks = 0; sim.tickMs = 0;
  }, 10000);
  console.log('s3: ready');
} catch (e) {
  fatal(e);
}

renderer.setAnimationLoop(frame);
