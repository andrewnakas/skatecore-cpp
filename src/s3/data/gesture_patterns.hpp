// Authored joystick PAT data consumed by TU3 82696B18 (skate-data gesture_patterns.rs),
// and the stock gameplay input.cfg compatibility check (input_config.rs).
#pragma once
#include <array>
#include <string>
#include <vector>

#include "result.hpp"
#include "vfs.hpp"

namespace s3::data {

// skate_core::input::gesture::Pattern (data shape only; recognition is core).
struct Pattern
{
	std::string name;
	std::vector<std::array<float, 2>> points;
	float tolerance_squared = 0;
	bool operator==(const Pattern &o) const
	{
		return name == o.name && points == o.points && tolerance_squared == o.tolerance_squared;
	}
};

// The pattern checks of skate_core Recognizer::new (2..=15 points, finite values,
// non-negative tolerance).
Status validate_recognizer_patterns(const std::vector<Pattern> &patterns);

namespace gesture_patterns {
// `path` is a VFS path, e.g. join_path(asset_root, "private/stock/data/joystick/skater.pat").
Result<std::vector<Pattern>> load(const Vfs &vfs, const std::string &path);
Result<std::vector<Pattern>> parse(const std::string &source);
} // namespace gesture_patterns

//! Compatibility check for the stock gameplay expressions specialized by
//! skate-core's GameplayActions. This is not the full native expression VM.
namespace input_config {
extern const char *const STOCK_INPUT_PATH; // "private/stock/data/config/input.cfg"
extern const char *const BUTTON_NAMES[24];
// TU3 82697740 registers these eighteen actions at indices 64..81.
extern const char *const GAMEPLAY[18][2];
} // namespace input_config

struct StockGameplayConfig
{
	static Result<StockGameplayConfig> load(const Vfs &vfs, const std::string &asset_root);
	/// Require the actual file to agree with the supported native mapping.
	/// A modified expression fails explicitly instead of silently running the
	/// stock specialization against different user data.
	static Result<StockGameplayConfig> validate(const std::string &source);
};

// Rust `str::lines()` (split on \n, strip one trailing \r per line, no final empty line).
std::vector<std::string> rust_lines(const std::string &text);
// Rust `split_whitespace()` (ASCII + common Unicode whitespace).
std::vector<std::string> rust_split_whitespace(const std::string &text);

} // namespace s3::data
