// Port of skate-core/src/physics/ground_hang_geometry.rs.
// Geometry actually observed by Ground::ManageHangUps82D39510.
// The caller reads only query completion, geometry kind104 and flag29 from
// 82C20C08; unrelated grind surface classifications are not used in this path.
#pragma once
#include <array>
#include <optional>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_world.hpp"

namespace s3::physics::ground_hang_geometry {

using board_world::BoardWorld;
using math::Vector3;

struct HangGeometryInput {
    /// Processed1312,1328 and1232, respectively.
    Vector3 edge_start;
    Vector3 edge_end;
    Vector3 reference_point;
};
struct HangLine {
    Vector3 start;
    Vector3 end;
    float radius = 0.0f;
};

/// Full six-line branch82C20728 used by82D397A0. Input's optional seventh
/// line byte is zeroed by82C20530 and never set by this ground caller.
std::optional<std::array<HangLine, 6>> hang_lines(HangGeometryInput input, float deck_center_to_truck);

/// 82C20ED4..82C20F84 and Ground's82D397EC..82D39810. We execute all six
/// native queries, then use the exact two classification fields read by Ground.
rs::Result<bool, const char*> detect_hung_geometry(const BoardWorld& world, HangGeometryInput input,
                                                   float deck_center_to_truck);

/// Rust-private, exposed for the ported test.
bool hung_classification(std::array<std::optional<float>, 6> hits);

}  // namespace s3::physics::ground_hang_geometry
