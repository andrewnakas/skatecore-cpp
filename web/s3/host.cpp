// Full-runtime web demo host (stage W1): the s3::data loaders in WebAssembly.
// JavaScript streams core.pack (and a map) straight into buffers allocated here, the VFS keeps
// views into them (no per-file copy), and the parsed .skate map is flattened into plain arrays
// that JS reads through HEAP views. Everything is single-threaded and lives until the next load.
#include <emscripten/emscripten.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "s3/data/animation_banks.hpp"
#include "s3/data/skate_map.hpp"
#include "s3/data/vfs.hpp"

using namespace s3::data;

namespace {

Vfs vfs;
bool vfs_ready = false;
std::shared_ptr<std::vector<uint8_t>> pending; // buffer JS is filling
std::string last_error;

// Flattened render geometry of the current map.
struct MapView
{
	SkateMap map;
	std::vector<float> positions, normals, uvs, uv1s; // per vertex: 3, 3, 2, 2 (uv1: lightmap)
	std::vector<uint32_t> indices;              // triangles sorted by material
	std::vector<uint32_t> groups;               // (material, first index, index count) triples
	std::vector<float> materials;               // MATERIAL_STRIDE floats each
	std::vector<float> lights;                  // LIGHT_STRIDE floats each
	float spawn[4] = {0, 0, 0, 0};              // x, y, z, heading
	bool spawn_moved = false;
};
std::unique_ptr<MapView> view;

// Rig hierarchy of the stock animation banks (OnBoard), for binding skins by name.
std::vector<std::string> rig_names;
std::vector<int32_t> rig_parents;

SharedBytes file_bytes; // last s3_read_file result (keeps the owner alive)

constexpr int MATERIAL_STRIDE = 16;
constexpr int LIGHT_STRIDE = 16;

int fail(const std::string &e)
{
	last_error = e;
	std::fprintf(stderr, "s3: %s\n", e.c_str());
	return 0;
}

} // namespace

extern "C" {

// ---- buffers and the VFS

// Allocate a zeroed buffer of `size` bytes for JS to fill; returns its address.
EMSCRIPTEN_KEEPALIVE uint8_t *s3_buffer(uint32_t size)
{
	pending = std::make_shared<std::vector<uint8_t>>(size);
	return pending->data();
}

// Install the filled buffer as an SKPK1 pack (replacing any previous VFS). Returns file count, 0 on error.
EMSCRIPTEN_KEEPALIVE int s3_load_pack()
{
	if (!pending) return fail("no buffer");
	Vfs fresh;
	auto r = fresh.install_pack(std::move(pending));
	pending.reset();
	if (!r.ok) return fail("pack: " + r.error);
	vfs = std::move(fresh);
	vfs_ready = true;
	return (int)vfs.file_count();
}

// Insert the filled buffer as a VFS file (e.g. "maps/KenneyPark.skate"). Returns 1 on success.
EMSCRIPTEN_KEEPALIVE int s3_add_file(const char *path)
{
	if (!pending) return fail("no buffer");
	SharedBytes bytes;
	bytes.data = pending->data();
	bytes.size = pending->size();
	bytes.owner = std::move(pending);
	pending.reset();
	vfs.add_file(path, std::move(bytes));
	return 1;
}

EMSCRIPTEN_KEEPALIVE const char *s3_error() { return last_error.c_str(); }
EMSCRIPTEN_KEEPALIVE int s3_file_count() { return (int)vfs.file_count(); }

// Read a VFS file without copying: returns its address and writes the size to *size (0 on error).
EMSCRIPTEN_KEEPALIVE const uint8_t *s3_read_file(const char *path, uint32_t *size)
{
	auto r = vfs.read(path);
	if (!r.ok) {
		fail(r.error);
		*size = 0;
		return nullptr;
	}
	file_bytes = r.value;
	*size = (uint32_t)file_bytes.size;
	return file_bytes.data;
}

// ---- the map

// Parse `path` from the VFS. Returns 1 on success.
EMSCRIPTEN_KEEPALIVE int s3_map_load(const char *path)
{
	view.reset();
	auto r = SkateMap::load(vfs, path);
	if (!r.ok) return fail("map: " + r.error);
	auto v = std::make_unique<MapView>();
	v->map = std::move(r.value);
	const SkateMap &m = v->map;
	const auto &verts = m.geometry.vertices;
	v->positions.reserve(verts.size() * 3);
	v->normals.reserve(verts.size() * 3);
	v->uvs.reserve(verts.size() * 2);
	v->uv1s.reserve(verts.size() * 2);
	for (const Vertex &x : verts) {
		v->positions.insert(v->positions.end(), x.position.begin(), x.position.end());
		v->normals.insert(v->normals.end(), x.normal.begin(), x.normal.end());
		v->uvs.insert(v->uvs.end(), x.uv.begin(), x.uv.end());
		v->uv1s.insert(v->uv1s.end(), x.lightmap_uv.begin(), x.lightmap_uv.end());
	}
	// Group triangles by the material of their first vertex (stable within a material).
	const auto &idx = m.geometry.indices;
	const size_t tris = idx.size() / 3;
	std::vector<uint32_t> order(tris);
	std::vector<uint32_t> mat(tris);
	for (size_t t = 0; t < tris; t++) {
		order[t] = (uint32_t)t;
		mat[t] = verts[idx[t * 3]].material;
	}
	std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return mat[a] < mat[b]; });
	v->indices.reserve(tris * 3);
	for (size_t i = 0; i < tris; i++) {
		const uint32_t t = order[i];
		if (i == 0 || mat[t] != mat[order[i - 1]]) v->groups.insert(v->groups.end(), {mat[t], (uint32_t)(i * 3), 0});
		v->groups.back() += 3;
		v->indices.insert(v->indices.end(), {idx[t * 3], idx[t * 3 + 1], idx[t * 3 + 2]});
	}
	// Materials: flags, friction, restitution, r, g, b, roughness, emissive,
	// albedo, indirect, normal, orm, emissive tex (1-based, 0 none), alpha mode, cutoff, indirect strength.
	for (const Material &x : m.materials) {
		const float row[MATERIAL_STRIDE] = {(float)x.flags, x.friction, x.restitution, x.color[0], x.color[1], x.color[2],
		                                    x.roughness, x.emissive, (float)x.textures[0], (float)x.textures[1],
		                                    (float)x.textures[2], (float)x.textures[3], (float)x.textures[4],
		                                    (float)x.alpha_mode, x.alpha_cutoff, x.indirect_strength};
		v->materials.insert(v->materials.end(), row, row + MATERIAL_STRIDE);
	}
	// Lights: kind, position, direction, color, intensity, range, radius, inner cos, outer cos, 0, 0.
	for (const Light &l : m.lights) {
		const float row[LIGHT_STRIDE] = {(float)l.kind, l.position[0], l.position[1], l.position[2], l.direction[0],
		                                 l.direction[1], l.direction[2], l.color[0], l.color[1], l.color[2],
		                                 l.intensity, l.range, l.radius, l.inner_cos, l.outer_cos, 0};
		v->lights.insert(v->lights.end(), row, row + LIGHT_STRIDE);
	}
	// Spawn as the game's startup does it (main.rs: grounded_spawn moves a spawn with no ground below).
	Vec3f spawn = m.spawn;
	auto g = spawn_ground::grounded_spawn(m, spawn);
	if (g.ok && g.value) {
		spawn = *g.value;
		v->spawn_moved = true;
	}
	v->spawn[0] = spawn[0];
	v->spawn[1] = spawn[1];
	v->spawn[2] = spawn[2];
	v->spawn[3] = m.heading;
	view = std::move(v);
	return 1;
}

EMSCRIPTEN_KEEPALIVE const char *s3_map_name() { return view ? view->map.name.c_str() : ""; }
EMSCRIPTEN_KEEPALIVE int s3_map_version() { return view ? view->map.version : 0; }
EMSCRIPTEN_KEEPALIVE int s3_map_vertex_count() { return view ? (int)view->map.geometry.vertices.size() : 0; }
EMSCRIPTEN_KEEPALIVE int s3_map_index_count() { return view ? (int)view->indices.size() : 0; }
EMSCRIPTEN_KEEPALIVE const float *s3_map_positions() { return view ? view->positions.data() : nullptr; }
EMSCRIPTEN_KEEPALIVE const float *s3_map_normals() { return view ? view->normals.data() : nullptr; }
EMSCRIPTEN_KEEPALIVE const float *s3_map_uvs() { return view ? view->uvs.data() : nullptr; }
EMSCRIPTEN_KEEPALIVE const float *s3_map_uv1s() { return view ? view->uv1s.data() : nullptr; }
EMSCRIPTEN_KEEPALIVE const uint32_t *s3_map_indices() { return view ? view->indices.data() : nullptr; }
EMSCRIPTEN_KEEPALIVE int s3_map_group_count() { return view ? (int)view->groups.size() / 3 : 0; }
EMSCRIPTEN_KEEPALIVE const uint32_t *s3_map_groups() { return view ? view->groups.data() : nullptr; }
EMSCRIPTEN_KEEPALIVE int s3_map_material_count() { return view ? (int)view->map.materials.size() : 0; }
EMSCRIPTEN_KEEPALIVE int s3_map_material_stride() { return MATERIAL_STRIDE; }
EMSCRIPTEN_KEEPALIVE const float *s3_map_materials() { return view ? view->materials.data() : nullptr; }
EMSCRIPTEN_KEEPALIVE const char *s3_map_material_name(int i)
{
	return view && i >= 0 && i < (int)view->map.materials.size() ? view->map.materials[i].name.c_str() : "";
}
EMSCRIPTEN_KEEPALIVE int s3_map_texture_count() { return view ? (int)view->map.textures.size() : 0; }
EMSCRIPTEN_KEEPALIVE const char *s3_map_texture_name(int i)
{
	return view && i >= 0 && i < (int)view->map.textures.size() ? view->map.textures[i].name.c_str() : "";
}
// width, height, color space (0 sRGB, 1 linear as authored).
EMSCRIPTEN_KEEPALIVE int s3_map_texture_info(int i, int field)
{
	if (!view || i < 0 || i >= (int)view->map.textures.size()) return 0;
	const Texture &t = view->map.textures[i];
	return field == 0 ? (int)t.width : field == 1 ? (int)t.height : (int)t.color_space;
}
EMSCRIPTEN_KEEPALIVE const uint8_t *s3_map_texture_rgba(int i)
{
	return view && i >= 0 && i < (int)view->map.textures.size() ? view->map.textures[i].rgba.data() : nullptr;
}
// x, y, z, heading (radians about +Y).
EMSCRIPTEN_KEEPALIVE const float *s3_map_spawn() { return view ? view->spawn : nullptr; }
EMSCRIPTEN_KEEPALIVE int s3_map_spawn_moved() { return view && view->spawn_moved; }
EMSCRIPTEN_KEEPALIVE int s3_map_environment_count() { return view ? (int)view->map.environment.size() : 0; }
EMSCRIPTEN_KEEPALIVE const float *s3_map_environment() { return view ? view->map.environment.data() : nullptr; }
EMSCRIPTEN_KEEPALIVE int s3_map_light_count() { return view ? (int)view->map.lights.size() : 0; }
EMSCRIPTEN_KEEPALIVE int s3_map_light_stride() { return LIGHT_STRIDE; }
EMSCRIPTEN_KEEPALIVE const float *s3_map_lights() { return view ? view->lights.data() : nullptr; }
EMSCRIPTEN_KEEPALIVE int s3_map_rail_count() { return view ? (int)view->map.rails.size() : 0; }
EMSCRIPTEN_KEEPALIVE int s3_map_collision_count() { return view ? (int)view->map.geometry.collision.size() : 0; }

// ---- the stock rig (ABIN hierarchy), for binding the skater skin by bone name

// Load the hierarchy of `<asset_root>/private/stock/data/anim/OnBoard.abin`. Returns the bone count.
EMSCRIPTEN_KEEPALIVE int s3_rig_load(const char *asset_root)
{
	auto banks = AnimationBanks::load(vfs, asset_root);
	if (!banks.ok) return fail("banks: " + banks.error);
	const abin::Hierarchy *h = banks->banks.at(0)->hierarchy();
	if (!h) return fail("OnBoard.abin has no hierarchy");
	rig_names = h->bone_names;
	rig_parents = h->parents;
	return (int)rig_names.size();
}
EMSCRIPTEN_KEEPALIVE const char *s3_rig_bone_name(int i) { return i >= 0 && i < (int)rig_names.size() ? rig_names[i].c_str() : ""; }
EMSCRIPTEN_KEEPALIVE int s3_rig_bone_parent(int i) { return i >= 0 && i < (int)rig_parents.size() ? rig_parents[i] : -1; }

} // extern "C"
