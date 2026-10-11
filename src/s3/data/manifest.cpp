// Asset manifest (manifest.rs) and input recordings (input_recording.rs).
#include "manifest.hpp"

#include "gesture_patterns.hpp"
#include "json.hpp"

namespace s3::data {

// Rust PathBuf::components() all Normal, no ':' '\\' '#', and the expected extension.
static Status validate_path(const std::string &value, const std::string &field, const std::string &extension)
{
	bool bad = value.empty() || value.find_first_of(":\\#") != std::string::npos || value[0] == '/';
	std::vector<std::string> parts;
	size_t b = 0;
	while (!bad && b <= value.size()) {
		size_t e = value.find('/', b);
		if (e == std::string::npos) e = value.size();
		std::string part = value.substr(b, e - b);
		if (part == "..") bad = true;
		else if (part == "." && b == 0) bad = true; // leading CurDir component
		else if (!part.empty() && part != ".") parts.push_back(part);
		b = e + 1;
	}
	if (!bad && parts.empty()) bad = true;
	if (bad) return Error{field + " must be a relative asset path using forward slashes"};
	const std::string &file = parts.back();
	size_t dot = file.rfind('.');
	std::string ext = (dot == std::string::npos || dot == 0) ? std::string() : file.substr(dot + 1);
	if (dot == std::string::npos || dot == 0 || ext != extension) return Error{field + " must name a ." + extension + " file"};
	return Unit{};
}

Result<GameAssets> GameAssets::parse(std::string_view json)
{
	auto wrap = [](const std::string &e) { return "Invalid game asset manifest: " + e; };
	S3_TRY_MAP(JsonDoc doc, JsonDoc::parse(json), wrap);
	JsonFields f(doc.root(), {"version", "character_scene", "initial_animation", "action_graph", "motion_graph"}, true);
	if (!f.ok()) return Error{wrap(f.error())};
	GameAssets m;
	S3_TRY_MAP(JsonValue v1, f.require("version"), wrap);
	S3_TRY_MAP(m.version, v1.to_u32(), wrap);
	S3_TRY_MAP(JsonValue v2, f.require("character_scene"), wrap);
	S3_TRY_MAP(m.character_scene, v2.to_string(), wrap);
	S3_TRY_MAP(JsonValue v3, f.require("initial_animation"), wrap);
	S3_TRY_MAP(m.initial_animation, v3.to_string(), wrap);
	S3_TRY_MAP(JsonValue v4, f.require("action_graph"), wrap);
	S3_TRY_MAP(m.action_graph, v4.to_string(), wrap);
	S3_TRY_MAP(JsonValue v5, f.require("motion_graph"), wrap);
	S3_TRY_MAP(m.motion_graph, v5.to_string(), wrap);
	if (m.version != 1) return Error{"Unsupported asset manifest version " + std::to_string(m.version)};
	S3_CHECK(validate_path(m.character_scene, "character_scene", "glb"));
	S3_CHECK(validate_path(m.action_graph, "action_graph", "stategraph"));
	S3_CHECK(validate_path(m.motion_graph, "motion_graph", "stategraph"));
	bool blank = true;
	for (const std::string &w : rust_split_whitespace(m.initial_animation)) blank &= w.empty();
	if (blank) return Error{"initial_animation must name a stock animation"};
	return m;
}

Result<GameAssets> GameAssets::load(const Vfs &vfs, const std::string &root)
{
	std::string file = join_path(root, "private/game.json");
	S3_TRY_MAP(std::string json, vfs.read_to_string(file), [&](const std::string &e) {
		return "Cannot read " + file + ": " + e + ". Run pipeline/Prepare-Assets.ps1 or select the correct --assets directory.";
	});
	S3_TRY(GameAssets manifest, parse(json));
	const std::pair<const char *, const std::string *> checks[] = {
		{"Character scene", &manifest.character_scene}, {"ActionGraph", &manifest.action_graph}, {"MotionGraph", &manifest.motion_graph}};
	for (auto &c : checks) {
		std::string path = join_path(root, *c.second);
		if (!vfs.is_file(path)) return Error{std::string(c.first) + " missing: " + path};
	}
	return manifest;
}

// ---- input_recording.rs ----

static Result<RecordingSample> parse_sample(std::string_view line)
{
	S3_TRY(JsonDoc doc, JsonDoc::parse(line));
	JsonFields f(doc.root(), {"schema", "t_us", "sample", "connected", "packet", "buttons", "triggers", "left", "right"}, true);
	if (!f.ok()) return Error{f.error()};
	RecordingSample s;
	S3_TRY(JsonValue a, f.require("schema"));
	S3_TRY(s.schema, a.to_u32());
	S3_TRY(JsonValue b, f.require("t_us"));
	S3_TRY(s.t_us, b.to_u64());
	S3_TRY(JsonValue c, f.require("sample"));
	S3_TRY(s.sample, c.to_u64());
	S3_TRY(JsonValue d, f.require("connected"));
	S3_TRY(s.connected, d.to_bool());
	JsonValue packet = f.get("packet"); // Option: absent or null is None
	if (packet.valid() && !packet.is_null()) {
		S3_TRY(uint32_t p, packet.to_u32());
		s.packet = p;
	}
	S3_TRY(JsonValue e, f.require("buttons"));
	S3_TRY(s.buttons, e.to_u16());
	auto pair = [](JsonValue v, auto &out, auto conv) -> Status {
		if (!v.is_array() || v.size() != 2) return Error{"invalid length, expected an array of length 2"};
		size_t i = 0;
		for (JsonValue x : v) {
			auto r = conv(x);
			if (!r.ok) return Error{r.error};
			out[i++] = r.value;
		}
		return Unit{};
	};
	S3_TRY(JsonValue t, f.require("triggers"));
	S3_CHECK(pair(t, s.triggers, [](JsonValue x) { return x.to_u8(); }));
	S3_TRY(JsonValue l, f.require("left"));
	S3_CHECK(pair(l, s.left, [](JsonValue x) { return x.to_i16(); }));
	S3_TRY(JsonValue r, f.require("right"));
	S3_CHECK(pair(r, s.right, [](JsonValue x) { return x.to_i16(); }));
	return s;
}

Result<Recording> Recording::read(const std::string &text)
{
	Recording out;
	std::vector<std::string> lines = rust_lines(text);
	for (size_t index = 0; index < lines.size(); index++) {
		const std::string &line = lines[index];
		std::string where = "Input recording line " + std::to_string(index + 1) + ": ";
		if (rust_split_whitespace(line).empty()) continue;
		S3_TRY_MAP(RecordingSample sample, parse_sample(line), [&](const std::string &e) { return where + e; });
		if (sample.schema != 1 || sample.sample != (uint64_t)out.samples_.size())
			return Error{where + "unsupported schema or missing/out-of-order sample"};
		if (!out.samples_.empty() && sample.t_us <= out.samples_.back().t_us) return Error{where + "timestamps must increase"};
		if (sample.connected != sample.packet.has_value()) return Error{where + "connection and packet disagree"};
		out.samples_.push_back(sample);
	}
	if (out.samples_.empty()) return Error{"Input recording is empty"};
	return out;
}

Result<Recording> Recording::load(const Vfs &vfs, const std::string &path)
{
	S3_TRY_MAP(std::string text, vfs.read_to_string(path), [&](const std::string &e) { return "Input recording " + path + ": " + e; });
	return read(text);
}

const RecordingSample &Recording::at_elapsed_us(uint64_t elapsed) const
{
	uint64_t first = samples_[0].t_us;
	uint64_t timestamp = elapsed > UINT64_MAX - first ? UINT64_MAX : first + elapsed;
	// partition_point(|s| s.t_us <= timestamp)
	size_t lo = 0, hi = samples_.size();
	while (lo < hi) {
		size_t mid = (lo + hi) / 2;
		if (samples_[mid].t_us <= timestamp) lo = mid + 1;
		else hi = mid;
	}
	return samples_[lo - 1];
}

} // namespace s3::data
