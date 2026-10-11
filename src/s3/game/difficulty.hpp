// Port of skate-game/src/difficulty.rs.
//! Host preference selects the native physics_mode collection, never a multiplier.
#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "s3/data/vfs.hpp"
#include "s3/game/common.hpp"

namespace s3::game::difficulty {

inline constexpr std::array<const char*, 5> NATIVE_MODES = {"easy", "normal", "hardcore", "motorized", "test"};

enum class Difficulty : std::uint32_t {
    Easy = 0,
    Normal = 1,
    Hardcore = 2,
};
inline constexpr std::array<Difficulty, 3> ALL = {Difficulty::Easy, Difficulty::Normal, Difficulty::Hardcore};

inline const char* key(Difficulty d) { return NATIVE_MODES[static_cast<std::size_t>(d)]; }
const char* label(Difficulty d);
rs::Result<Difficulty, std::string> parse(const std::string& value);
/// `root.parent().unwrap_or(root).join("settings/gameplay.json")`.
std::string path(const std::string& root);
/// Reads the saved settings from `vfs` (absent -> Easy, like NotFound in the game).
rs::Result<Difficulty, std::string> load(const sd::Vfs& vfs, const std::string& root);
/// Serde lowercase name (the saved JSON value).
const char* serde_name(Difficulty d);
/// `serde_json::from_slice::<Saved>` (`{"difficulty":"easy"}`).
rs::Result<Difficulty, std::string> parse_saved(const std::string& json);
/// `serde_json::to_vec(&Saved { difficulty })`.
std::string saved_json(Difficulty d);

}  // namespace s3::game::difficulty
