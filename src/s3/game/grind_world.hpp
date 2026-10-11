// Port of skate-game/src/grind_world.rs.
//! Authored test-course grind paths. Collision/visuals share these dimensions.
//! This asset owner does not claim a nearby spline is an acquired grind.
// SIMREF: bevy removed; GrindGeometryPlugin removed.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/skate_map.hpp"
#include "s3/game/grind_world/derive.hpp"
#include "s3/game/grind_world/octree.hpp"
#include "s3/game/grind_world/provider.hpp"
#include "s3/game/grind_world/spline.hpp"

namespace s3::game::grind_world {

namespace rs = ::s3::rs;
using ::s3::math::Vector3;
using provider::SourceIdentity;
using provider::StaticProvider;
using spline::PrimitiveMetadata;
using spline::primitives;

struct Rail {
    std::uint64_t id = 0;
    const char* name = "";  // &'static str
    Vector3 start{};
    Vector3 end{};
    float width = 0.0f;
    float thickness = 0.0f;
};

/// Shared authored geometry. The physical grind owner runs native truck queries
/// against these same spline endpoints before state selection.
struct GrindGeometry {
    std::vector<Rail> rails;
    std::vector<std::uint8_t> native_blob;

    static GrindGeometry for_world(bool test_world);
};

std::array<Rail, 7> rails();

/// Box rails have their spline on the top centre, not the centre of the tube.
/// Coping is flush with the deck lip; the existing transition remains intact.
std::vector<std::array<Vector3, 4>> surfaces();

// Private in Rust.
void box_faces(std::vector<std::array<Vector3, 4>>& out, Vector3 min, Vector3 max);

/// Retain the archive's relocatable Pegasus representation, including all 120
/// authored bytes per segment. Omitted runtime pointers are rebuilt as offsets.
/// (`map: Option<&SkateMap>` -> nullable pointer.)
rs::Result<std::vector<std::uint8_t>, std::string> native_blob(const ::s3::data::SkateMap* map);

std::vector<std::uint8_t> spline_blob();
std::vector<::s3::data::Rail> test_rails();

}  // namespace s3::game::grind_world
