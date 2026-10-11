// Full-runtime web demo host (stage W1): the s3::data loaders in WebAssembly.
// JavaScript streams core.pack (and a map) straight into buffers allocated here, the VFS keeps
// views into them (no per-file copy), and the parsed .skate map is flattened into plain arrays
// that JS reads through HEAP views. Everything is single-threaded and lives until the next load.
#include <emscripten/emscripten.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "s3/data/animation_banks.hpp"
#include "s3/data/skate_map.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/driver.hpp"

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

// The simulation (stage W2): s3::game::driver::Sim over the VFS + the loaded map.
std::unique_ptr<s3::game::driver::Sim> sim;
s3::game::driver::TickRecord record;
std::string events_joined;
std::string trick_identifier;
// View of the last tick, see s3_sim_view for the layout.
constexpr int VIEW_FLOATS = 64;
float view_buf[VIEW_FLOATS];
std::vector<float> bone_buf; // 16 floats per bone (column-major)
float board_buf[7 * 13];     // per body: position 3, orientation 4 (xyzw), linear velocity 3, angular velocity 3

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
	sim.reset(); // the simulation refers to the map
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
		std::fprintf(stderr, "SKATE_SPAWN_MOVED from=[%g, %g, %g] to=[%g, %g, %g] (no ground below)\n", spawn[0], spawn[1],
		             spawn[2], (*g.value)[0], (*g.value)[1], (*g.value)[2]);
		spawn = *g.value;
		v->spawn_moved = true;
		v->map.spawn = spawn; // the simulation starts there too
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

// ---- the simulation (stage W2)

namespace {
void fill_view()
{
	const auto p = sim->presentation();
	float *v = view_buf;
	std::memset(v, 0, sizeof(view_buf));
	for (int c = 0; c < 4; c++)
		for (int r = 0; r < 4; r++) v[c * 4 + r] = p.root[c][r];
	v[16] = p.camera_position[0];
	v[17] = p.camera_position[1];
	v[18] = p.camera_position[2];
	for (int c = 0; c < 3; c++)
		for (int r = 0; r < 3; r++) v[19 + c * 3 + r] = p.camera_basis[c][r];
	v[28] = p.field_of_view_degrees;
	v[29] = p.has_camera ? 1.f : 0.f;
	v[30] = p.camera_discontinuity ? 1.f : 0.f;
	v[31] = p.teleport ? 1.f : 0.f;
	v[32] = (float)p.period_seconds;
	v[33] = (float)(p.pose_generation & 0xffffff);
	v[34] = p.board_speed;
	v[35] = (float)p.wheel_surface;
	v[36] = (float)p.grind_audio_surface;
	v[37] = p.powersliding ? 1.f : 0.f;
	v[38] = p.footstep_strength;
	v[39] = p.foot_push_speed;
	v[40] = record.grounded ? 1.f : 0.f;
	v[41] = record.wiping_out ? 1.f : 0.f;
	v[42] = record.landed ? 1.f : 0.f;
	v[43] = (float)record.contact_count;
	v[44] = (float)record.physical_state_id;
	v[45] = (float)record.tick;
	v[46] = p.sequence_score;
	v[47] = p.multiplier;
	v[48] = p.line_score;
	v[49] = p.total_score;
	v[50] = record.rider_root_position[0];
	v[51] = record.rider_root_position[1];
	v[52] = record.rider_root_position[2];
	v[53] = record.rider_root_velocity[0];
	v[54] = record.rider_root_velocity[1];
	v[55] = record.rider_root_velocity[2];
	const auto &bones = *p.bones;
	bone_buf.resize(bones.size() * 16);
	for (size_t b = 0; b < bones.size(); b++)
		for (int c = 0; c < 4; c++)
			for (int r = 0; r < 4; r++) bone_buf[b * 16 + c * 4 + r] = bones[b][c][r];
	for (size_t i = 0; i < record.board.size() && i < 7; i++) {
		const auto &b = record.board[i];
		float *o = board_buf + i * 13;
		std::memcpy(o, b.position.data(), 12);
		std::memcpy(o + 3, b.orientation.data(), 16);
		std::memcpy(o + 7, b.linear_velocity.data(), 12);
		std::memcpy(o + 10, b.angular_velocity.data(), 12);
	}
	events_joined.clear();
	for (const auto &e : record.events) {
		events_joined += e;
		events_joined += '\n';
	}
	trick_identifier = p.trick_identifier;
}
} // namespace

// Start the simulation on the map `path` (parsed by s3_map_load if it is not the current map;
// nullptr or "" = the test world). Uses the installed pack VFS. Returns 1 on success.
EMSCRIPTEN_KEEPALIVE int s3_sim_load(const char *path)
{
	sim.reset();
	record = {};
	const SkateMap *map = nullptr;
	if (path && *path) {
		if (!view && !s3_map_load(path)) return 0;
		map = &view->map;
	}
	try {
		auto r = s3::game::driver::Sim::load(vfs, "assets", map, nullptr);
		if (r.is_err()) return fail("sim: " + r.unwrap_err());
		sim = std::move(r.unwrap());
	} catch (const std::exception &e) {
		sim.reset();
		return fail(std::string("sim load panic: ") + e.what());
	}
	return 1;
}

// One fixed tick with a raw XInput sample. Returns 1 on success (0: failed, see s3_error).
EMSCRIPTEN_KEEPALIVE int s3_sim_step(int buttons, int lx, int ly, int rx, int ry, int lt, int rt)
{
	if (!sim) return fail("no simulation");
	s3::game::driver::Pad pad;
	pad.buttons = (uint16_t)buttons;
	pad.lx = (int16_t)std::clamp(lx, -32768, 32767);
	pad.ly = (int16_t)std::clamp(ly, -32768, 32767);
	pad.rx = (int16_t)std::clamp(rx, -32768, 32767);
	pad.ry = (int16_t)std::clamp(ry, -32768, 32767);
	pad.left_trigger = (uint8_t)std::clamp(lt, 0, 255);
	pad.right_trigger = (uint8_t)std::clamp(rt, 0, 255);
	try {
		auto r = sim->step(pad);
		if (r.is_err()) {
			sim.reset();
			return fail("step: " + r.unwrap_err());
		}
		record = std::move(r.unwrap());
		fill_view();
	} catch (const std::exception &e) {
		sim.reset();
		return fail(std::string("step panic: ") + e.what());
	}
	return 1;
}

// camera::present: the window aspect ratio (width / height) applied after each tick.
EMSCRIPTEN_KEEPALIVE void s3_sim_set_aspect(float aspect)
{
	if (sim) sim->set_aspect_ratio(aspect);
}
EMSCRIPTEN_KEEPALIVE float s3_sim_time_step() { return sim ? sim->time_step() : 0.f; }
// Layout: [0..15] root (animation_to_world, column-major), [16..18] camera position,
// [19..27] camera basis columns right/up/at, [28] fov degrees, [29] has camera, [30] camera discontinuity,
// [31] teleport, [32] fixed period s, [33] pose generation, [34] board speed, [35] wheel audio surface,
// [36] grind audio surface, [37] powersliding, [38] footstep strength, [39] foot push speed, [40] grounded,
// [41] wiping out, [42] landed, [43] contacts, [44] physical state id, [45] tick, [46] sequence score,
// [47] multiplier, [48] line score, [49] completed lines score, [50..52] rider root position, [53..55] its velocity.
EMSCRIPTEN_KEEPALIVE const float *s3_sim_view() { return view_buf; }
EMSCRIPTEN_KEEPALIVE int s3_sim_bone_count() { return (int)bone_buf.size() / 16; }
EMSCRIPTEN_KEEPALIVE const float *s3_sim_bones() { return bone_buf.data(); }
// 7 bodies (wheel_rf, wheel_lf, wheel_rb, wheel_lb, truck_f, truck_b, deck) x 13 floats.
EMSCRIPTEN_KEEPALIVE const float *s3_sim_board() { return board_buf; }
EMSCRIPTEN_KEEPALIVE const char *s3_sim_state_name() { return record.physical_state.c_str(); }
EMSCRIPTEN_KEEPALIVE const char *s3_sim_motion_state() { return record.motion_state.c_str(); }
EMSCRIPTEN_KEEPALIVE const char *s3_sim_action_state() { return record.action_state.c_str(); }
EMSCRIPTEN_KEEPALIVE const char *s3_sim_clip() { return record.animation_clip.c_str(); }
EMSCRIPTEN_KEEPALIVE const char *s3_sim_trick_name() { return record.trick_name.c_str(); }
EMSCRIPTEN_KEEPALIVE const char *s3_sim_trick_identifier() { return trick_identifier.c_str(); }
EMSCRIPTEN_KEEPALIVE const char *s3_sim_score_name() { return record.score_name.c_str(); }
// This tick's events, one per line (STATE:a>b, LANDING, WIPEOUT, TRICK:x, SCORE:x, AIRBORNE, LANDED:typeN, BAIL, ...).
EMSCRIPTEN_KEEPALIVE const char *s3_sim_events() { return events_joined.c_str(); }

} // extern "C"
