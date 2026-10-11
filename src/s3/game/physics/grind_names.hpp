// Port of skate-game/src/physics/grind_names.rs (also mounted as physics::grind_chromosome::names).
//! ZIP short names verified entry-for-entry from original S3 initializer82DE6678.
//!82F92AC0 splits384 records;82DEE508 publishes skating IDs and scoring IDs
//! separately. S2 82E2F9D0 uses160 records (two orientations, five families).
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>

namespace s3::game::physics::grind_names {

struct Name {
    std::int32_t skating_id = 0;
    const char* attribute = "";  // &'static str
    const char* display = "";    // &'static str
    std::int32_t scorable_id = 0;
};
[[maybe_unused]] inline bool operator==(const Name& a, const Name& b) {
    return a.skating_id == b.skating_id && std::strcmp(a.attribute, b.attribute) == 0 &&
           std::strcmp(a.display, b.display) == 0 && a.scorable_id == b.scorable_id;
}
[[maybe_unused]] inline bool operator!=(const Name& a, const Name& b) { return !(a == b); }

// `#[path = "grind_names/tableN.rs"] mod tableN;` -> generated src/s3/game/physics/grind_names/tableN.cpp.
namespace table0 { extern const std::array<Name, 48> TABLE; }
namespace table1 { extern const std::array<Name, 48> TABLE; }
namespace table2 { extern const std::array<Name, 48> TABLE; }
namespace table3 { extern const std::array<Name, 48> TABLE; }
namespace table4 { extern const std::array<Name, 48> TABLE; }
namespace table5 { extern const std::array<Name, 48> TABLE; }
namespace table6 { extern const std::array<Name, 48> TABLE; }
namespace table7 { extern const std::array<Name, 48> TABLE; }

/// Dimensions: approach2, location2, twist2, tilt2, orientation4, family6.
/// Invalid/sentinel chromosomes are not silently mapped to a fifty-fifty.
std::optional<Name> lookup(std::array<std::uint32_t, 6> c);

}  // namespace s3::game::physics::grind_names
