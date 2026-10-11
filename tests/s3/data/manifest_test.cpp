// Ported tests: skate-data tests/manifest.rs and tests/input_recording.rs; VFS/SKPK1 checks.
#include "s3/data/manifest.hpp"
#include "s3/data/vfs.hpp"
#include "test.hpp"

using namespace s3::data;

namespace {
std::string manifest(const std::string &path, uint32_t version, const std::string &action = "private/stock/data/state/ActionGraph_OnBoard.stategraph",
                     const std::string &motion = "private/stock/data/state/MotionGraph_OnBoard.stategraph")
{
	auto esc = [](const std::string &s) {
		std::string o;
		for (char c : s) {
			if (c == '\\' || c == '"') o += '\\';
			o += c;
		}
		return o;
	};
	return "{\"version\":" + std::to_string(version) + ",\"character_scene\":\"" + esc(path) +
	       "\",\"initial_animation\":\"R_IDLE_HCOM_000\",\"action_graph\":\"" + esc(action) + "\",\"motion_graph\":\"" + esc(motion) + "\"}";
}
} // namespace

// tests/manifest.rs: validates_contract_without_loading_private_assets
S3_TEST(manifest_validates_contract_without_loading_private_assets)
{
	auto m = GameAssets::parse(manifest("private/skater.glb", 1));
	CHECK_OK(m);
	CHECK_EQ(m->initial_animation, std::string("R_IDLE_HCOM_000"));
}

// tests/manifest.rs: rejects_incompatible_versions_and_ambiguous_paths
S3_TEST(manifest_rejects_incompatible_versions_and_ambiguous_paths)
{
	CHECK(!GameAssets::parse(manifest("private/skater.glb", 2)).ok);
	for (const char *path : {"../skater.glb", "/skater.glb", "C:/skater.glb", "private\\skater.glb", "private/skater.glb#Scene0", "private/skater.json", ""})
		CHECK_MSG(!GameAssets::parse(manifest(path, 1)).ok, std::string("accepted ") + path);
}

// tests/manifest.rs: requires_relative_compiled_stock_graph_paths
S3_TEST(manifest_requires_relative_compiled_stock_graph_paths)
{
	std::string action = "private/stock/data/state/ActionGraph_OnBoard.stategraph";
	std::string motion = "private/stock/data/state/MotionGraph_OnBoard.stategraph";
	for (int field = 0; field < 2; field++) {
		for (const char *path : {"../graph.stategraph", "/graph.stategraph", "graph.xml", ""}) {
			std::string json = field == 0 ? manifest("private/skater.glb", 1, path, motion) : manifest("private/skater.glb", 1, action, path);
			CHECK_MSG(!GameAssets::parse(json).ok, std::string("accepted ") + path);
		}
		(field == 0 ? action : motion) = field == 0 ? "private/action_graph.stategraph" : "private/motion_graph.stategraph";
	}
	CHECK(GameAssets::parse(manifest("private/skater.glb", 1, action, motion)).ok);
}

// tests/manifest.rs: reports_missing_data_as_an_error
S3_TEST(manifest_reports_missing_data_as_an_error)
{
	Vfs empty;
	auto r = GameAssets::load(empty, "skate-missing-assets");
	CHECK(!r.ok);
	CHECK(r.error.find("private") != std::string::npos && r.error.find("Prepare-Assets") != std::string::npos);
}

namespace {
std::string line(uint64_t sample, uint64_t t_us, bool connected, int16_t right_y)
{
	return "{\"schema\":1,\"sample\":" + std::to_string(sample) + ",\"t_us\":" + std::to_string(t_us) + ",\"connected\":" +
	       (connected ? "true" : "false") + ",\"packet\":" + (connected ? "5" : "null") +
	       ",\"buttons\":0,\"triggers\":[0,0],\"left\":[0,0],\"right\":[0," + std::to_string(right_y) + "]}\n";
}
std::string replace(std::string s, const std::string &from, const std::string &to)
{
	size_t at = s.find(from);
	if (at != std::string::npos) s.replace(at, from.size(), to);
	return s;
}
} // namespace

// tests/input_recording.rs: irregular_timing_keeps_edges_axes_and_disconnects
S3_TEST(input_recording_irregular_timing_keeps_edges_axes_and_disconnects)
{
	std::string text = line(0, 18000, true, -32768) + line(1, 60000, true, 32767) + line(2, 65000, false, 0);
	auto recording = Recording::read(text);
	CHECK_OK(recording);
	CHECK_EQ(recording->duration_us(), (uint64_t)47000);
	CHECK_EQ(recording->at_elapsed_us(41999).raw_state().right[1], (int16_t)-32768);
	CHECK_EQ(recording->at_elapsed_us(42000).raw_state().right[1], (int16_t)32767);
	CHECK(!recording->at_elapsed_us(47000).connected);
	CHECK(!recording->at_elapsed_us(UINT64_MAX).connected);
}

// tests/input_recording.rs: truncated_misordered_and_unknown_capture_formats_fail_explicitly
S3_TEST(input_recording_truncated_misordered_and_unknown_capture_formats_fail_explicitly)
{
	for (const std::string &text : {std::string(), line(0, 10, true, 0) + "{", line(1, 10, true, 0), line(0, 10, true, 0) + line(1, 9, true, 0),
	                                 replace(line(0, 10, true, 0), "\"schema\":1", "\"schema\":2"),
	                                 replace(line(0, 10, true, 0), "\"packet\":5", "\"packet\":null")})
		CHECK_MSG(!Recording::read(text).ok, "accepted " + text);
}

S3_TEST(vfs_skpk1_pack_install_and_lookup)
{
	std::vector<uint8_t> pack = {'S', 'K', 'P', 'K', '1', 0, 0, 0};
	auto entry = [&](const std::string &name, const std::string &data) {
		s3test::put_u32(pack, (uint32_t)name.size());
		pack.insert(pack.end(), name.begin(), name.end());
		s3test::put_u32(pack, (uint32_t)data.size());
		s3test::put_u32(pack, 0);
		pack.insert(pack.end(), data.begin(), data.end());
	};
	s3test::put_u32(pack, 3);
	entry("assets/private/game.json", "{}");
	entry("assets/private/stock/data/joystick/skater.pat", "x");
	entry("settings/default-map.json", "\"maps/KenneyPark.skate\"");
	auto vfs = Vfs::from_pack(pack);
	CHECK_OK(vfs);
	CHECK_EQ(vfs->file_count(), (size_t)3);
	CHECK(vfs->is_file("assets/private/game.json"));
	CHECK(vfs->is_file("./assets//private/../private/game.json"));
	CHECK(vfs->is_dir("assets/private/stock"));
	CHECK_EQ(vfs->read_to_string(join_path("assets", "private/game.json")).value, std::string("{}"));
	auto dir = vfs->read_dir("assets/private");
	CHECK_OK(dir);
	CHECK(dir.value == std::vector<std::string>({"game.json", "stock"}));
	CHECK_ERR_CONTAINS(vfs->read("assets/missing.json"), "/assets/missing.json: not found");
	vfs->add_file("maps/KenneyPark.skate", std::vector<uint8_t>{1, 2, 3});
	CHECK_EQ(vfs->read("maps/KenneyPark.skate").value.size, (size_t)3);
	// Truncated / unsafe packs fail.
	std::vector<uint8_t> truncated(pack.begin(), pack.end() - 1);
	CHECK_ERR_CONTAINS(Vfs::from_pack(truncated), "Truncated pack");
	std::vector<uint8_t> unsafe = {'S', 'K', 'P', 'K', '1', 0, 0, 0};
	s3test::put_u32(unsafe, 1);
	std::string bad = "a/../b";
	s3test::put_u32(unsafe, (uint32_t)bad.size());
	unsafe.insert(unsafe.end(), bad.begin(), bad.end());
	s3test::put_u32(unsafe, 0);
	s3test::put_u32(unsafe, 0);
	CHECK_ERR_CONTAINS(Vfs::from_pack(unsafe), "Unsafe pack path a/../b");
	CHECK_ERR_CONTAINS(Vfs::from_pack(std::vector<uint8_t>{'X'}), "Truncated pack");
}
