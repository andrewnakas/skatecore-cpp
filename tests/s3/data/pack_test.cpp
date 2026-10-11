// Integration tests against the real cleanroom pack (~/Documents/haloskate-ref/cleanpack/core.pack,
// KenneyPark.skate; S3_PACK / S3_MAP override). Skipped (with a note) when the files are absent.
// Expected counts/values were computed independently from the pack bytes (Python walk of the
// SKPK1 entries, lookup8 identities, ABIN record headers, stategraph trees and PAT directives).
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <memory>

#include "s3/data/animation_banks.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/gesture_patterns.hpp"
#include "s3/data/manifest.hpp"
#include "s3/data/physics_skeleton.hpp"
#include "s3/data/scoring.hpp"
#include "s3/data/skate_map.hpp"
#include "s3/data/state_graph.hpp"
#include "test.hpp"

using namespace s3::data;

namespace {

const std::string ROOT = "assets"; // pack paths are `assets/private/...`

double ms_since(std::chrono::steady_clock::time_point t0)
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

// Loaded once and shared by the pack tests.
struct Pack
{
	bool tried = false;
	std::unique_ptr<Vfs> vfs;
	std::unique_ptr<Collections> collections;
	std::unique_ptr<AnimationBanks> banks;
};
Pack &pack()
{
	static Pack p;
	if (!p.tried) {
		p.tried = true;
		std::vector<uint8_t> bytes;
		std::string path = s3test::pack_path();
		if (path.empty() || !s3test::read_file(path, bytes)) {
			std::printf("  (skip: no pack at %s)\n", path.c_str());
			return p;
		}
		auto t0 = std::chrono::steady_clock::now();
		auto vfs = Vfs::from_pack(std::move(bytes));
		if (!vfs.ok) {
			std::printf("  pack install failed: %s\n", vfs.error.c_str());
			return p;
		}
		std::printf("  pack: %zu files installed in %.1f ms\n", vfs->file_count(), ms_since(t0));
		p.vfs = std::make_unique<Vfs>(std::move(vfs.value));
	}
	return p;
}

} // namespace

S3_TEST(pack_skater_collections)
{
	Pack &p = pack();
	if (!p.vfs) return;
	auto t0 = std::chrono::steady_clock::now();
	auto data = Collections::load(*p.vfs, ROOT);
	CHECK_OK(data);
	std::printf("  skater-collections.json: %zu collections, parsed+validated in %.1f ms\n", data->entries().size(), ms_since(t0));
	CHECK_EQ(data->entries().size(), (size_t)6849);
	CHECK_EQ(data->version, 1u);
	struct Q
	{
		const char *cls, *key, *name;
		uint32_t bits;
	};
	for (const Q &q : {Q{"anim_motion", "manual", "manual_clamp_vel", 0x3D23D70Au}, Q{"anim_motion", "jumping", "clamp_low_inclusive", 0x3EFD70A4u},
	                   Q{"anim_motion", "bumps", "min_bump_mag", 0x42C80000u}, Q{"anim_handplant", "default", "Hash_512342FB1D4A7D57", 0x3F07AE14u}}) {
		auto v = data->float_(q.cls, q.key, q.name);
		CHECK_MSG(v.ok, v.error);
		std::printf("  %s/%s/%s = %.9g (0x%08X)\n", q.cls, q.key, q.name, v.value, f32_to_bits(v.value));
		CHECK_EQ(f32_to_bits(v.value), q.bits);
	}
	auto collide = data->boolean("physics_skeleton", "default", "EnableCollision");
	CHECK_OK(collide);
	CHECK(collide.value);
	auto drives = data->boolean("animation", "default", "Use_Drives");
	CHECK_OK(drives);
	CHECK(drives.value);
	// Scoring data resolves every authored scorable and the tuning curves.
	auto scoring = ScoringData::load(data.value);
	CHECK_OK(scoring);
	std::printf("  scoring: %zu scorable definitions, combo levels %.3g/%.3g %.3g/%.3g %.3g/%.3g, line capacity %.6g\n",
	            scoring->definitions.size(), scoring->combo_levels[0].first, scoring->combo_levels[0].second, scoring->combo_levels[1].first,
	            scoring->combo_levels[1].second, scoring->combo_levels[2].first, scoring->combo_levels[2].second, scoring->line_capacity);
	CHECK(!scoring->definitions.empty());
	CHECK(scoring->by_id(scoring->definitions[0].metadata.id) == &scoring->definitions[0]);
	p.collections = std::make_unique<Collections>(std::move(data.value));
}

S3_TEST(pack_game_manifest_and_input_config)
{
	Pack &p = pack();
	if (!p.vfs) return;
	auto manifest = GameAssets::load(*p.vfs, ROOT);
	CHECK_OK(manifest);
	CHECK_EQ(manifest->initial_animation, std::string("R_IDLE_HCOM_000"));
	auto input = StockGameplayConfig::load(*p.vfs, ROOT);
	CHECK_OK(input);
}

S3_TEST(pack_animation_banks_metadata_and_frames)
{
	Pack &p = pack();
	if (!p.vfs) return;
	auto t0 = std::chrono::steady_clock::now();
	auto banks = AnimationBanks::load(*p.vfs, ROOT);
	CHECK_OK(banks);
	std::printf("  banks loaded+hashed in %.1f ms\n", ms_since(t0));
	CHECK_EQ(banks->identities[0], std::string("30aa324d6d7c51c325d53e9268c1ad91783b0154d21bbef5dc5a61eae8333bd7"));
	CHECK_EQ(banks->identities[1], std::string("4c7e33fd054b621f24ca9394d29b82ee40108700a7b8cc6ce1dc9aa4f5a5e00d"));
	// Record type histogram from the bank headers: OnBoard {2:2672, 3:87, 4:1, 5:1, 6:2, 7:1001, 8:98, 11:11},
	// OffBoard {2:652, 3:105, 4:1, 5:1, 7:182, 11:27}.
	const size_t expected[2][12] = {{0, 0, 2672, 87, 1, 1, 2, 1001, 98, 0, 0, 11}, {0, 0, 652, 105, 1, 1, 0, 182, 0, 0, 0, 27}};
	const char *names[2] = {"OnBoard", "OffBoard"};
	for (int b = 0; b < 2; b++) {
		size_t counts[12] = {0};
		size_t clips = 0, poses = 0;
		for (const abin::Record &r : banks->banks[b]->records()) {
			if (r.header.type_id < 12) counts[r.header.type_id]++;
			clips += std::holds_alternative<abin::Clip>(r.data);
			poses += std::holds_alternative<abin::Pose>(r.data);
		}
		const abin::Hierarchy *h = banks->banks[b]->hierarchy();
		CHECK(h != nullptr);
		std::printf("  %s.abin: %zu records, %zu clips, %zu poses, %zu trees; hierarchy %u bones, %zu parts, trajectory %d\n", names[b],
		            banks->banks[b]->records().size(), clips, poses, counts[6] + counts[7] + counts[8] + counts[11], (unsigned)h->bone_count,
		            h->parts.size(), (int)h->has_trajectory);
		for (int t = 0; t < 12; t++) CHECK_MSG(counts[t] == expected[b][t], std::string(names[b]) + " type " + std::to_string(t));
	}
	auto metadata = banks->metadata();
	CHECK_OK(metadata);
	std::printf("  metadata: %zu clips across %zu bank sources\n", metadata->clip_count(), metadata->sources().size());
	CHECK_EQ(metadata->clip_count(), (size_t)(2672 + 652));
	auto idle = metadata->clip("R_IDLE_HCOM_000");
	CHECK_OK(idle);
	std::printf("  R_IDLE_HCOM_000: fps %.6g, frames %.6g, %zu attributes, bank %s\n", f32_from_bits(idle.value->fps_bits),
	            f32_from_bits(idle.value->frames_bits), idle.value->attributes.size(), metadata->source_for("R_IDLE_HCOM_000")->source_bank.c_str());

	auto frames = AnimationFrames::from_banks(banks.value);
	CHECK_OK(frames);
	std::printf("  native frames: %zu distinct clips\n", frames->clip_count());
	CHECK_EQ(frames->clip_count(), (size_t)(2672 + 652));
	t0 = std::chrono::steady_clock::now();
	auto clip = frames->clip("r_idle_hcom_000");
	CHECK_OK(clip);
	const ClipFrames &c = *clip.value;
	CHECK_EQ((float)c.frames.size(), f32_from_bits(idle.value->frames_bits));
	CHECK_EQ(c.frames[0].size(), frames->bone_names.size());
	// Decoded quaternions are close to unit length (bits are not renormalized).
	double worst = 0;
	for (const auto &frame : c.frames)
		for (const SampleWords &s : frame) {
			double q = 0;
			for (int i = 3; i < 7; i++) q += (double)f32_from_bits(s[i]) * f32_from_bits(s[i]);
			worst = std::max(worst, std::fabs(std::sqrt(q) - 1.0));
		}
	std::printf("  decoded %zu frames x %zu bones in %.1f ms; max |quat|-1 = %.3g; root t0 = (%.6g, %.6g, %.6g)\n", c.frames.size(),
	            c.frames[0].size(), ms_since(t0), worst, f32_from_bits(c.frames[0][0][7]), f32_from_bits(c.frames[0][0][8]),
	            f32_from_bits(c.frames[0][0][9]));
	CHECK(worst < 0.02);
	auto tpose = frames->named_pose("RIG_TPOSE");
	CHECK_OK(tpose);
	CHECK_EQ(tpose.value->samples.size(), frames->bone_names.size());
	// Decode every clip of both banks once (exercises every VBR stream).
	t0 = std::chrono::steady_clock::now();
	size_t total_frames = 0, failures = 0;
	std::string first_error;
	for (const std::string &name : frames->clip_names()) {
		auto r = frames->clip(name);
		if (!r.ok) {
			if (first_error.empty()) first_error = name + ": " + r.error;
			failures++;
			continue;
		}
		total_frames += r.value->frames.size();
	}
	std::printf("  decoded all %zu clips (%zu frames) in %.1f ms, %zu failures %s\n", frames->clip_names().size(), total_frames, ms_since(t0),
	            failures, first_error.c_str());
	CHECK_MSG(failures == 0, first_error);

	// physics-skeletons.json is keyed by the OnBoard bank identity.
	auto skeleton = PhysicsSkeleton::load(*p.vfs, join_path(ROOT, "private/stock/physics-skeletons.json"), frames->source_sha256, "PHYS_TPOSE");
	CHECK_OK(skeleton);
	std::printf("  physics skeleton %s: %zu bones, bone0 %s size (%.9g, %.9g, %.9g)\n", skeleton->name.c_str(), skeleton->bones.size(),
	            skeleton->bones[0].name.c_str(), skeleton->bones[0].size()[0], skeleton->bones[0].size()[1], skeleton->bones[0].size()[2]);
	CHECK_EQ(skeleton->bones.size(), (size_t)24);
	CHECK_EQ(skeleton->bones[0].name, std::string("SKATEBOARD_ROOT"));
	CHECK_EQ(skeleton->bones[0].size()[0], 0.2432f);
	CHECK_ERR_CONTAINS(PhysicsSkeleton::load(*p.vfs, join_path(ROOT, "private/stock/physics-skeletons.json"), banks->identities[1], "PHYS_TPOSE"),
	                   "identity or format differs");
	p.banks = std::make_unique<AnimationBanks>(std::move(banks.value));
}

S3_TEST(pack_state_graphs)
{
	Pack &p = pack();
	if (!p.vfs) return;
	// Element counts per graph from an independent tree walk of the pack bytes.
	const std::pair<const char *, size_t> expected[] = {
		{"ActionGraph_OnBoard.stategraph", 1608},
		{"MotionGraph_OnBoard.stategraph", 16297},
		{"livingworldentities/marquee/Motiongraph_Photographer.stategraph", 71},
		{"livingworldentities/marquee/Photographer.stategraph", 27},
		{"livingworldentities/marquee/marquee.stategraph", 17},
		{"livingworldentities/marquee/motiongraph_marquee.stategraph", 44},
		{"livingworldentities/pedestrian/MotionGraph_Pedestrian.stategraph", 2914},
		{"livingworldentities/pedestrian/Pedestrian.stategraph", 684},
		{"livingworldentities/pedestrian/plugin/actortracker_spectate.stategraph", 47},
		{"livingworldentities/pedestrian/plugin/conversation.stategraph", 60},
		{"livingworldentities/pedestrian/plugin/lookat.stategraph", 9},
		{"livingworldentities/pedestrian/plugin/sit.stategraph", 57},
		{"livingworldentities/pedestrian/plugin/spectate.stategraph", 49},
		{"livingworldentities/pedestrian/plugin/useatm.stategraph", 43},
		{"livingworldentities/pedestrian/plugin/usenewspaperbox.stategraph", 44},
		{"livingworldentities/pedestrian/plugin/usetrashbin.stategraph", 8},
		{"livingworldentities/pedestrian/plugin/usevendingmachine.stategraph", 47},
		{"livingworldentities/pedestrian/plugin/usewaterfountain.stategraph", 51},
		{"livingworldentities/vehicle/Vehicle.stategraph", 68},
	};
	std::string dir = join_path(ROOT, "private/stock/data/state");
	size_t files = 0;
	for (const std::string &path : p.vfs->files_under(dir)) {
		if (path.size() < 11 || path.compare(path.size() - 11, 11, ".stategraph") != 0) continue;
		files++;
		auto t0 = std::chrono::steady_clock::now();
		auto graph = StateGraph::load(*p.vfs, path);
		CHECK_MSG(graph.ok, graph.error);
		auto binding = state_graph_binding::Binding::from_graph(graph.value);
		std::string rel = path.substr(dir.size() + 1);
		std::printf("  %s: %zu elements (%.1f ms); binding %s", rel.c_str(), graph->elements.size(), ms_since(t0),
		            binding.ok ? "ok" : binding.error.c_str());
		if (binding.ok)
			std::printf(" (%zu states, %zu transitions, %zu expressions, %zu operations)", binding->states.size(), binding->transitions.size(),
			            binding->expressions.size(), binding->operations.size());
		std::printf("\n");
		bool known = false;
		for (auto &e : expected)
			if (rel == e.first) {
				known = true;
				CHECK_MSG(graph->elements.size() == e.second, rel);
			}
		CHECK_MSG(known, rel);
		// The game binds the two skater graphs (graph_runtime.rs load_graph).
		if (rel == "ActionGraph_OnBoard.stategraph" || rel == "MotionGraph_OnBoard.stategraph") CHECK_MSG(binding.ok, binding.error);
	}
	CHECK_EQ(files, sizeof expected / sizeof expected[0]);
}

S3_TEST(pack_joystick_patterns)
{
	Pack &p = pack();
	if (!p.vfs) return;
	const std::pair<const char *, size_t> expected[] = {{"skater.pat", 78},       {"skater90.pat", 78}, {"skaterN90.pat", 78},
	                                                     {"skater_air.pat", 21},   {"skater_fingerflip.pat", 14},
	                                                     {"skaterls.pat", 10},     {"skaterstep.pat", 6}};
	std::string dir = join_path(ROOT, "private/stock/data/joystick");
	size_t files = 0;
	for (const std::string &path : p.vfs->files_under(dir)) {
		if (path.size() < 4 || path.compare(path.size() - 4, 4, ".pat") != 0) continue;
		files++;
		auto patterns = gesture_patterns::load(*p.vfs, path);
		CHECK_MSG(patterns.ok, patterns.error);
		std::string rel = path.substr(dir.size() + 1);
		std::printf("  %s: %zu patterns; first %s (%zu points, tolerance^2 %.9g)\n", rel.c_str(), patterns->size(), patterns->front().name.c_str(),
		            patterns->front().points.size(), patterns->front().tolerance_squared);
		for (auto &e : expected)
			if (rel == e.first) CHECK_MSG(patterns->size() == e.second, rel);
	}
	CHECK_EQ(files, (size_t)7);
}

S3_TEST(pack_kenney_park_map)
{
	std::vector<uint8_t> bytes;
	std::string path = s3test::map_path();
	if (path.empty() || !s3test::read_file(path, bytes)) {
		std::printf("  (skip: no map at %s)\n", path.c_str());
		return;
	}
	// The web demo adds the fetched map to the VFS next to the pack.
	Vfs vfs;
	vfs.add_file("maps/KenneyPark.skate", std::move(bytes));
	auto t0 = std::chrono::steady_clock::now();
	auto map = SkateMap::load(vfs, "maps/KenneyPark.skate");
	CHECK_OK(map);
	std::printf("  %s v%u: %zu materials, %zu textures, %zu vertices, %zu indices, %zu collision triangles, %zu rails, %zu extensions (%.1f ms)\n",
	            map->name.c_str(), (unsigned)map->version, map->materials.size(), map->textures.size(), map->geometry.vertices.size(),
	            map->geometry.indices.size(), map->geometry.collision.size(), map->rails.size(), map->extensions.size(), ms_since(t0));
	std::printf("  spawn (%.9g, %.9g, %.9g) heading %.9g\n", map->spawn[0], map->spawn[1], map->spawn[2], map->heading);
	CHECK_EQ(map->version, (uint8_t)14);
	CHECK_EQ(map->name, std::string("kenney_park"));
	CHECK_EQ(map->materials.size(), (size_t)9);
	CHECK_EQ(map->textures.size(), (size_t)1);
	CHECK_EQ(map->textures[0].rgba.size(), (size_t)512 * 512 * 4);
	CHECK_EQ(map->geometry.vertices.size(), (size_t)3572);
	CHECK_EQ(map->geometry.indices.size(), (size_t)5802);
	CHECK_EQ(map->geometry.collision.size(), (size_t)1921);
	CHECK_EQ(map->rails.size(), (size_t)2);
	CHECK(map->spawn == (Vec3f{13.5f, 0.0f, -1.5f}));
	CHECK_EQ(f32_to_bits(map->heading), 0x40490fdbu);
	auto grounded = spawn_ground::grounded_spawn(map.value, map->spawn);
	CHECK_OK(grounded);
	if (grounded->has_value())
		std::printf("  grounded spawn moved to (%.9g, %.9g, %.9g)\n", (**grounded)[0], (**grounded)[1], (**grounded)[2]);
	else
		std::printf("  grounded spawn: authored spawn has ground below\n");
}

// Presentation props are loaded with parse_render_only (skate-game retail_backdrop.rs).
S3_TEST(pack_native_prop_maps_render_only)
{
	Pack &p = pack();
	if (!p.vfs) return;
	for (const char *name : {"BlackBoxPark", "MaloofMoneyCup"}) {
		std::string path = join_path(ROOT, std::string("private/native-props/") + name + ".skate");
		auto t0 = std::chrono::steady_clock::now();
		auto bytes = p.vfs->read(path);
		CHECK_OK(bytes);
		auto map = SkateMap::parse_render_only(bytes->data, bytes->size);
		CHECK_MSG(map.ok, map.error);
		size_t texels = 0;
		for (const Texture &t : map->textures) texels += t.rgba.size() / 4;
		std::printf("  %s v%u: %zu materials, %zu textures (%zu texels), %zu vertices, %zu triangles (%.1f ms)\n", map->name.c_str(),
		            (unsigned)map->version, map->materials.size(), map->textures.size(), texels, map->geometry.vertices.size(),
		            map->geometry.indices.size() / 3, ms_since(t0));
		// Playable loading still demands collision.
		CHECK_ERR_CONTAINS(SkateMap::parse(bytes->data, bytes->size), "requires triangle collision");
	}
}

// Optional: S3_RETAIL_MAPS=path1:path2 (retail cleanroom maps with embedded RWCM collision).
S3_TEST(retail_maps_with_rwcm_collision_optional)
{
	const char *env = std::getenv("S3_RETAIL_MAPS");
	if (!env || !*env) {
		std::printf("  (skip: set S3_RETAIL_MAPS to .skate paths)\n");
		return;
	}
	std::string list = env;
	size_t b = 0;
	while (b <= list.size()) {
		size_t e = list.find(':', b);
		if (e == std::string::npos) e = list.size();
		std::string path = list.substr(b, e - b);
		b = e + 1;
		if (path.empty()) continue;
		std::vector<uint8_t> bytes;
		CHECK_MSG(s3test::read_file(path, bytes), path);
		auto t0 = std::chrono::steady_clock::now();
		auto map = SkateMap::parse(bytes.data(), bytes.size());
		CHECK_MSG(map.ok, path + ": " + map.error);
		double parse_ms = ms_since(t0);
		size_t rwcm = 0, clusters = 0;
		for (const Extension &ext : map->extensions) {
			if (!ext.tag_is("RWCM")) continue;
			auto n = retail_collision::visit_clusters(ext.payload.data(), ext.payload.size(),
			                                          [&](const std::string &, const std::vector<RetailTriangle> &) -> Status {
				                                          clusters++;
				                                          return Unit{};
			                                          });
			CHECK_MSG(n.ok, n.error);
			rwcm += n.value;
		}
		auto grounded = spawn_ground::grounded_spawn(map.value, map->spawn);
		CHECK_OK(grounded);
		std::printf("  %s v%u: %zu textures, %zu vertices, %zu collision, RWCM %zu triangles / %zu clusters, spawn (%.6g, %.6g, %.6g)%s (parse %.0f ms)\n",
		            map->name.c_str(), (unsigned)map->version, map->textures.size(), map->geometry.vertices.size(), map->geometry.collision.size(),
		            rwcm, clusters, map->spawn[0], map->spawn[1], map->spawn[2], grounded->has_value() ? " -> regrounded" : "", parse_ms);
		CHECK(map->geometry.collision.size() + rwcm > 0);
	}
}
