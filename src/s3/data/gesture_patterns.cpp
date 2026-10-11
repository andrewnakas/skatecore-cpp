// Joystick PAT loader (gesture_patterns.rs) and input.cfg validation (input_config.rs).
#include "gesture_patterns.hpp"

#include <map>
#include <optional>

namespace s3::data {

std::vector<std::string> rust_lines(const std::string &text)
{
	std::vector<std::string> out;
	size_t start = 0;
	while (start < text.size()) {
		size_t nl = text.find('\n', start);
		size_t end = nl == std::string::npos ? text.size() : nl;
		std::string line = text.substr(start, end - start);
		// Only "\r\n" is a line ending; a bare final '\r' stays.
		if (nl != std::string::npos && !line.empty() && line.back() == '\r') line.pop_back();
		out.push_back(std::move(line));
		if (nl == std::string::npos) break;
		start = nl + 1;
	}
	return out;
}

// Length of a Unicode White_Space character at s[i], or 0.
static size_t whitespace_len(const std::string &s, size_t i)
{
	unsigned char c = (unsigned char)s[i];
	if (c == ' ' || (c >= 0x09 && c <= 0x0D)) return 1;
	if (c == 0xC2 && i + 1 < s.size() && ((unsigned char)s[i + 1] == 0x85 || (unsigned char)s[i + 1] == 0xA0)) return 2;
	if (c == 0xE1 && i + 2 < s.size() && (unsigned char)s[i + 1] == 0x9A && (unsigned char)s[i + 2] == 0x80) return 3;
	if (c == 0xE2 && i + 2 < s.size()) {
		unsigned char b = (unsigned char)s[i + 1], d = (unsigned char)s[i + 2];
		if (b == 0x80 && (d <= 0x8A || d == 0xA8 || d == 0xA9 || d == 0xAF)) return 3;
		if (b == 0x81 && d == 0x9F) return 3;
	}
	if (c == 0xE3 && i + 2 < s.size() && (unsigned char)s[i + 1] == 0x80 && (unsigned char)s[i + 2] == 0x80) return 3;
	return 0;
}

std::vector<std::string> rust_split_whitespace(const std::string &text)
{
	std::vector<std::string> out;
	std::string word;
	size_t i = 0;
	while (i < text.size()) {
		size_t w = whitespace_len(text, i);
		if (w) {
			if (!word.empty()) out.push_back(std::move(word));
			word.clear();
			i += w;
		} else {
			word += text[i++];
		}
	}
	if (!word.empty()) out.push_back(std::move(word));
	return out;
}

Status validate_recognizer_patterns(const std::vector<Pattern> &patterns)
{
	for (const Pattern &pattern : patterns) {
		bool bad = pattern.points.size() < 2 || pattern.points.size() > 15 || !is_finite(pattern.tolerance_squared) ||
		           pattern.tolerance_squared < 0.0f;
		for (const auto &p : pattern.points) bad |= !is_finite(p[0]) || !is_finite(p[1]);
		if (bad) return Error{"invalid native gesture pattern " + pattern.name};
	}
	return Unit{};
}

namespace gesture_patterns {

Result<std::vector<Pattern>> load(const Vfs &vfs, const std::string &path)
{
	S3_TRY_MAP(std::string text, vfs.read_to_string(path), [&](const std::string &e) { return path + ": " + e; });
	S3_TRY_MAP(std::vector<Pattern> p, parse(text), [&](const std::string &e) { return path + ": " + e; });
	return p;
}

Result<std::vector<Pattern>> parse(const std::string &source_text)
{
	std::vector<Pattern> patterns;
	std::optional<float> tolerance;
	std::vector<std::string> lines = rust_lines(source_text);
	for (size_t line = 0; line < lines.size(); line++) {
		const std::string &source = lines[line];
		size_t comment = source.find("//");
		std::vector<std::string> words = rust_split_whitespace(comment == std::string::npos ? source : source.substr(0, comment));
		if (words.empty()) continue;
		auto fail = [&]() { return Error{"invalid PAT directive at line " + std::to_string(line + 1) + ": " + source}; };
		auto number = [&](size_t index) -> Result<float> {
			if (index >= words.size()) return fail();
			float value;
			if (!parse_f32(words[index], value)) return fail();
			if (!is_finite(value)) return fail();
			return value;
		};
		const std::string &w0 = words[0];
		if (w0 == "global_tolerance_dist" && words.size() == 2) {
			S3_TRY(float v, number(1));
			tolerance = v * v;
		} else if ((w0 == "global_tolerance_time" || w0 == "global_tolerance_speed" || w0 == "global_anticipation_delay" ||
		            w0 == "tolerance_time") &&
		           words.size() == 2) {
			// The native loader reads but does not store these values.
			S3_CHECK(number(1));
		} else if (w0 == "pattern" && words.size() == 2) {
			if (!tolerance) return Error{"PAT lacks global_tolerance_dist"};
			patterns.push_back(Pattern{words[1], {}, *tolerance});
		} else if (w0 == "tolerance_dist" && words.size() == 2) {
			S3_TRY(float v, number(1));
			if (patterns.empty()) return fail();
			patterns.back().tolerance_squared = v * v;
		} else if (w0 == "coord" && words.size() == 3) {
			S3_TRY(float x, number(1));
			S3_TRY(float y, number(2));
			if (patterns.empty()) return fail();
			patterns.back().points.push_back({x, y});
		} else {
			// Fail explicitly on polar or modified directives until their
			// transcendental implementation is provided. Stock skater.pat uses coord.
			return fail();
		}
	}
	S3_CHECK(validate_recognizer_patterns(patterns));
	return patterns;
}

} // namespace gesture_patterns

// ---- input_config.rs ----

namespace input_config {
const char *const STOCK_INPUT_PATH = "private/stock/data/config/input.cfg";
const char *const BUTTON_NAMES[24] = {
	"DPadU", "DPadD", "DPadL", "DPadR", "Start", "Back", "LStick", "RStick", "LBumper", "RBumper", "LTrigger", "RTrigger",
	"A", "B", "X", "Y", "LStickR", "LStickL", "LStickU", "LStickD", "RStickR", "RStickL", "RStickU", "RStickD",
};
const char *const GAMEPLAY[18][2] = {
	{"GP_LStickX", "LStickR-LStickL"}, {"GP_LStickY", "LStickU-LStickD"}, {"GP_LStickIn", "LStick"},
	{"GP_RStickX", "RStickR-RStickL"}, {"GP_RStickY", "RStickU-RStickD"}, {"GP_RStickIn", "RStick"},
	{"GP_LTrigger", "LTrigger"},       {"GP_RTrigger", "RTrigger"},       {"GP_LBumper", "LBumper"},
	{"GP_RBumper", "RBumper"},         {"GP_UDPad", "DPadU"},             {"GP_DDPad", "DPadD"},
	{"GP_LDPad", "DPadL"},             {"GP_RDPad", "DPadR"},             {"GP_XFace", "X"},
	{"GP_YFace", "Y"},                 {"GP_AFace", "A"},                 {"GP_BFace", "B"},
};
} // namespace input_config

// Rust str::trim (Unicode whitespace).
static std::string rust_trim(const std::string &s)
{
	size_t b = 0, e = s.size();
	for (;;) {
		size_t w = b < e ? whitespace_len(s, b) : 0;
		if (!w) break;
		b += w;
	}
	// Trailing: scan forward recording the end of the last non-whitespace char.
	size_t last = b, i = b;
	while (i < e) {
		size_t w = whitespace_len(s, i);
		if (w) i += w;
		else last = ++i;
	}
	return s.substr(b, last - b);
}

static Status require_definition(const std::map<std::string, std::string> &definitions, const std::string &name, const std::string &expected)
{
	auto it = definitions.find(name);
	if (it == definitions.end()) return Error{"Missing gameplay input definition " + name};
	if (it->second == expected) return Unit{};
	return Error{"Unsupported " + name + " = " + it->second + "; the implemented stock mapping requires " + expected};
}

Result<StockGameplayConfig> StockGameplayConfig::load(const Vfs &vfs, const std::string &asset_root)
{
	std::string path = join_path(asset_root, input_config::STOCK_INPUT_PATH);
	S3_TRY_MAP(std::string source, vfs.read_to_string(path), [&](const std::string &e) { return "Cannot read " + path + ": " + e; });
	S3_TRY_MAP(StockGameplayConfig c, validate(source), [&](const std::string &e) { return "Input configuration " + path + ": " + e; });
	return c;
}

Result<StockGameplayConfig> StockGameplayConfig::validate(const std::string &source_in)
{
	std::string source;
	std::vector<std::string> lines = rust_lines(source_in);
	for (size_t i = 0; i < lines.size(); i++) {
		if (i) source += '\n';
		size_t c = lines[i].find("//");
		source += c == std::string::npos ? lines[i] : lines[i].substr(0, c);
	}
	std::map<std::string, std::string> definitions;
	size_t start = 0;
	for (;;) {
		size_t semi = source.find(';', start);
		std::string statement = source.substr(start, semi == std::string::npos ? std::string::npos : semi - start);
		size_t eq = statement.find('=');
		if (eq != std::string::npos) {
			std::string name = rust_trim(statement.substr(0, eq));
			bool known = false;
			for (const char *b : input_config::BUTTON_NAMES) known |= name == b;
			for (auto &g : input_config::GAMEPLAY) known |= name == g[0];
			if (known) {
				std::string expression;
				std::string rest = statement.substr(eq + 1);
				size_t i = 0;
				while (i < rest.size()) {
					size_t w = whitespace_len(rest, i);
					if (w) i += w;
					else expression += rest[i++];
				}
				if (!definitions.emplace(name, expression).second) return Error{"Duplicate gameplay input definition " + name};
			}
		}
		if (semi == std::string::npos) break;
		start = semi + 1;
	}
	for (size_t index = 0; index < 24; index++)
		S3_CHECK(require_definition(definitions, input_config::BUTTON_NAMES[index], "Button" + std::to_string(index)));
	for (auto &g : input_config::GAMEPLAY) S3_CHECK(require_definition(definitions, g[0], g[1]));
	return StockGameplayConfig{};
}

} // namespace s3::data
