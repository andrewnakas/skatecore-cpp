// skatecore s3 web demo, stage W1: the s3::data loaders (WebAssembly) read the cleanroom pack and a
// .skate map; three.js draws the decoded map and the skater (bind pose, on its board, at the spawn).
// The pack is fetched at run time from the cleanroom site and never bundled with this page.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import Skate3 from './skate3.js';

const $ = (id) => document.getElementById(id);
const params = new URLSearchParams(location.search);
// Same-origin path on GitHub Pages; the absolute URL elsewhere (Pages sends Access-Control-Allow-Origin: *).
const BASE = location.hostname === 'andrewnakas.github.io'
  ? '/skate3-cleanroom/'
  : 'https://andrewnakas.github.io/skate3-cleanroom/';
const ASSET_ROOT = 'assets';
const SKATER_GLB = 'assets/private/skater.glb';

function status(text) { $('status').textContent = text; console.log('s3:', text); }
function fatal(e) {
  console.error(e);
  $('status').textContent = String(e && e.message || e);
  $('status').className = 'error';
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
  const want = (params.get('map') || 'KenneyPark').toLowerCase();
  const map = manifest.maps.find((m) => m.name.toLowerCase() === want) || manifest.maps.find((m) => m.name === 'KenneyPark');
  const nav = $('maps');
  for (const m of manifest.maps) {
    const a = document.createElement('a');
    a.href = `?map=${encodeURIComponent(m.name)}`;
    a.textContent = m.name;
    a.title = `${(m.size / 1048576).toFixed(1)} MB`;
    if (m === map) a.className = 'on';
    nav.appendChild(a);
  }

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
  return { map, manifest };
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
    `${lights.length / ls} lights; spawn ${[...sp.subarray(0, 3)].map((v) => v.toFixed(2))} heading ${sp[3].toFixed(3)}` +
    `${core._s3_map_spawn_moved() ? ' (grounded)' : ''}; environment [${[...env].map((v) => +v.toFixed(3))}]`);
  for (const m of materials) {
    console.log(`s3: material ${m.name} map=${m.map?.name || '-'} lightmap=${m.lightMap?.name || '-'} ` +
      `normal=${m.normalMap?.name || '-'} orm=${m.roughnessMap?.name || '-'} rough=${m.roughness.toFixed(2)}`);
  }
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
const camera = new THREE.PerspectiveCamera(60, innerWidth / innerHeight, 0.05, 3000);
const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;
addEventListener('resize', () => {
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
});

function light(world) {
  const e = world.env;
  if (e.length >= 6) scene.background = new THREE.Color().setRGB(e[3], e[4], e[5]);
  const hour = e.length > 10 ? ((e[10] % 24) + 24) % 24 : 12;
  const d = e.length > 11 ? daylight(e, hour) : daylight(new Float32Array(12), 12);
  // Ambient: Bevy adds brightness · exposure · albedo; three.js divides ambient by π.
  scene.add(new THREE.AmbientLight(0xffffff, d.ambient * BEVY_EXPOSURE * Math.PI));
  const sun = new THREE.DirectionalLight(d.color, d.illuminance * BEVY_EXPOSURE);
  sun.position.copy(world.spawn).addScaledVector(d.direction, 80);
  sun.target.position.copy(world.spawn);
  sun.castShadow = true;
  sun.shadow.mapSize.set(2048, 2048);
  const s = sun.shadow.camera;
  s.left = s.bottom = -50; s.right = s.top = 50; s.near = 1; s.far = 200;
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
  console.log(`s3: hour ${hour.toFixed(2)} sun dir ${d.direction.toArray().map((v) => v.toFixed(2))} ` +
    `${d.illuminance.toFixed(0)} lux, ambient ${d.ambient.toFixed(0)}`);
}

// ---------- the skater (bind pose; the simulation drives it from W2)
async function buildSkater(world) {
  const bytes = readFile(SKATER_GLB);
  const gltf = await new Promise((resolve, reject) => new GLTFLoader().parse(bytes.buffer, '', resolve, reject));
  const root = gltf.scene;
  // The GLB's joint nodes are all identity: its rest pose lives only in the inverse bind matrices,
  // so the bones must be posed from them (Skeleton.pose) before anything is drawn.
  const skinned = [];
  root.traverse((o) => { if (o.isSkinnedMesh) skinned.push(o); });
  root.updateMatrixWorld(true);
  skinned[0].skeleton.pose();
  root.updateMatrixWorld(true);
  const box = new THREE.Box3();
  for (const m of skinned) {
    m.castShadow = true;
    m.receiveShadow = true;
    m.frustumCulled = false;
    m.computeBoundingBox(); // applies the current bone transforms
    box.union(m.boundingBox);
  }
  // The game puts the wheels' contact point at the spawn height (physics.rs: wheel bottom = map.spawn.y),
  // so the board's lowest point goes exactly there (no raycast: overhead geometry would catch it).
  root.position.set(world.spawn.x, world.spawn.y - box.min.y, world.spawn.z);
  root.rotation.y = world.heading;
  scene.add(root);
  const bones = skinned[0].skeleton.bones.map((b) => b.name);
  console.log(`s3: skater.glb ${(bytes.length / 1048576).toFixed(1)} MB, ${skinned.length} skinned primitives, ` +
    `${bones.length} joints, size ${box.getSize(new THREE.Vector3()).toArray().map((v) => v.toFixed(2))} m, ` +
    `box min ${box.min.toArray().map((v) => v.toFixed(2))} max ${box.max.toArray().map((v) => v.toFixed(2))}`);
  console.log(`s3: glb joints: ${bones.join(' ')}`);
  return { root, skinned, bones, box };
}

function compareRig(glbBones) {
  const n = core._s3_rig_load ? withCString(ASSET_ROOT, (p) => core._s3_rig_load(p)) : 0;
  if (!n) { console.warn('s3: rig', str(core._s3_error())); return; }
  const names = [], parents = [];
  for (let i = 0; i < n; i++) { names.push(str(core._s3_rig_bone_name(i))); parents.push(core._s3_rig_bone_parent(i)); }
  console.log(`s3: ABIN rig ${n} bones: ${names.map((s, i) => `${i}:${s}<${parents[i]}`).join(' ')}`);
  const upper = names.map((s) => s.toUpperCase());
  const map = glbBones.map((b) => upper.indexOf(b.toUpperCase()));
  console.log(`s3: glb joint -> ABIN bone: ${map.join(',')}`);
  const missing = glbBones.filter((b, i) => map[i] < 0);
  const unused = names.filter((s) => !glbBones.some((b) => b.toUpperCase() === s.toUpperCase()));
  console.log(`s3: glb joints absent from the rig: [${missing}]; rig bones without a glb joint: [${unused}]`);
  window.s3rig = { names, parents, glbToRig: map };
}

// ---------- main
try {
  const { map } = await load();
  status('building scene…');
  const world = buildMap();
  scene.add(world.mesh);
  light(world);
  const skater = await buildSkater(world);
  const target = skater.root.position.clone().add(new THREE.Vector3(0, 0.9, 0));
  controls.target.copy(target);
  const back = new THREE.Vector3(Math.sin(world.heading), 0, Math.cos(world.heading));
  camera.position.copy(target).addScaledVector(back, -3.2).add(new THREE.Vector3(1.2, 0.8, 0));
  controls.update();
  $('loading').classList.add('done');
  $('info').textContent = `${map.name} · stage W1: map + skater (bind pose), orbit camera`;
  compareRig(skater.bones);
  try { console.log('s3: exposure.json', new TextDecoder().decode(readFile('assets/private/exposure.json'))); } catch (e) { /* optional */ }
  window.s3 = { core, scene, camera, controls, world, skater, THREE };
  console.log('s3: ready');
} catch (e) {
  fatal(e);
}

renderer.setAnimationLoop(() => {
  controls.update();
  renderer.render(scene, camera);
});
