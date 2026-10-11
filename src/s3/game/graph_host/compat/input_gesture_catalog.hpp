// Compat for skate-game/src/input/gesture_catalog.rs (crate::input, not yet ported).
// FALLBACK OWNED BY STAGE A: delete once s3/game/input/gesture_catalog.hpp exists.
#pragma once
#if __has_include("s3/game/input/gesture_catalog.hpp")
#include "s3/game/input/gesture_catalog.hpp"
#else
#include <array>
#include <string>
#include <string_view>

#include "s3/core/rs.hpp"

namespace s3::game::input::gesture_catalog {
//! Original TU3 gesture membership,82BA10C0 ->82BA07F0.
//! The catalog is constructed by82B98DB8 ->82B98F70, independently checked
//! against all270 raw map-insertion calls. PAT paths do not define these groups.

// Square82B99060..99DAC; Tail82B99E20..9AAD0; Nose82B9AB40..9B7F0.
// Tail90 map5676,82B9B864..9BE84; TailN90 map9460,82B9CC18..9D238;
// Nose90 map3784,82B9DFCC..9E5EC; NoseN90 map7568,82B9F380..9F9A0.
inline constexpr std::array<const char*, 30> COMMON = {
    "Ollie",
    "PopShuvit",
    "FSPopShuvit",
    "VarialKickflip",
    "VarialHeelflip",
    "Hardflip",
    "InwardHeelflip",
    "360PopShuvit",
    "FS360PopShuvit",
    "360Flip",
    "Laserflip",
    "360Hardflip",
    "360InwardHeelflip",
    "Kickflip",
    "Heelflip",
    "Nollie",
    "N_PopShuvit",
    "N_FSPopShuvit",
    "N_VarialKickflip",
    "N_VarialHeelflip",
    "N_Hardflip",
    "N_InwardHeelflip",
    "N_360PopShuvit",
    "N_FS360PopShuvit",
    "N_360Flip",
    "N_Laserflip",
    "N_360Hardflip",
    "N_360InwardHeelflip",
    "N_Kickflip",
    "N_Heelflip",
};
inline constexpr std::array<const char*, 15> TAIL_90 = {
    "90_Ollie",
    "90_PopShuvit",
    "90_FSPopShuvit",
    "90_VarialKickflip",
    "90_VarialHeelflip",
    "90_Hardflip",
    "90_InwardHeelflip",
    "90_360PopShuvit",
    "90_FS360PopShuvit",
    "90_360Flip",
    "90_Laserflip",
    "90_360Hardflip",
    "90_360InwardHeelflip",
    "90_Kickflip",
    "90_Heelflip",
};
inline constexpr std::array<const char*, 15> TAIL_N90 = {
    "N90_Ollie",
    "N90_PopShuvit",
    "N90_FSPopShuvit",
    "N90_VarialKickflip",
    "N90_VarialHeelflip",
    "N90_Hardflip",
    "N90_InwardHeelflip",
    "N90_360PopShuvit",
    "N90_FS360PopShuvit",
    "N90_360Flip",
    "N90_Laserflip",
    "N90_360Hardflip",
    "N90_360InwardHeelflip",
    "N90_Kickflip",
    "N90_Heelflip",
};
inline constexpr std::array<const char*, 15> NOSE_90 = {
    "90_Nollie",
    "90_N_PopShuvit",
    "90_N_FSPopShuvit",
    "90_N_VarialKickflip",
    "90_N_VarialHeelflip",
    "90_N_Hardflip",
    "90_N_InwardHeelflip",
    "90_N_360PopShuvit",
    "90_N_FS360PopShuvit",
    "90_N_360Flip",
    "90_N_Laserflip",
    "90_N_360Hardflip",
    "90_N_360InwardHeelflip",
    "90_N_Kickflip",
    "90_N_Heelflip",
};
inline constexpr std::array<const char*, 15> NOSE_N90 = {
    "N90_Nollie",
    "N90_N_PopShuvit",
    "N90_N_FSPopShuvit",
    "N90_N_VarialKickflip",
    "N90_N_VarialHeelflip",
    "N90_N_Hardflip",
    "N90_N_InwardHeelflip",
    "N90_N_360PopShuvit",
    "N90_N_FS360PopShuvit",
    "N90_N_360Flip",
    "N90_N_Laserflip",
    "N90_N_360Hardflip",
    "N90_N_360InwardHeelflip",
    "N90_N_Kickflip",
    "N90_N_Heelflip",
};

enum class Group {
    Square,
    Nose,
    Tail,
    Nose90,
    Tail90,
    NoseN90,
    TailN90,
};

/// Original82BA0F78 does case-sensitive constructor comparisons.
inline rs::Result<Group, std::string> group_parse(std::string_view name) {
    if (name == "Square") return Group::Square;
    if (name == "Nose") return Group::Nose;
    if (name == "Tail") return Group::Tail;
    if (name == "90Nose") return Group::Nose90;
    if (name == "90Tail") return Group::Tail90;
    if (name == "N90Nose") return Group::NoseN90;
    if (name == "N90Tail") return Group::TailN90;
    return rs::make_err("HasGestureIntent has undefined group `" + std::string(name) + "`");
}

///82BA07F0 tests map-key presence, never the intent value. All seven maps
///contain the same30 unrotated keys; each rotated map adds its15 own keys.
template <class F>
inline bool group_has_intent(Group self, F&& has) {
    for (const char* name : COMMON)
        if (has(name)) return true;
    const std::array<const char*, 15>* additional = nullptr;
    switch (self) {
        case Group::Square:
        case Group::Nose:
        case Group::Tail: return false;
        case Group::Nose90: additional = &NOSE_90; break;
        case Group::Tail90: additional = &TAIL_90; break;
        case Group::NoseN90: additional = &NOSE_N90; break;
        case Group::TailN90: additional = &TAIL_N90; break;
    }
    for (const char* name : *additional)
        if (has(name)) return true;
    return false;
}

}  // namespace s3::game::input::gesture_catalog
#endif
