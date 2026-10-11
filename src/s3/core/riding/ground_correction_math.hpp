// Port of skate-core/src/riding/ground_correction_math.rs.
// Remaining numerical ground corrections from TU3 82D38800 and its callees.
// These operate on actual physical vectors; phase/counter gates stay in the
// grounded state owner and all body writes target the shared board solver.
#pragma once
#include <array>

#include "s3/core/math.hpp"
#include "s3/core/physics/assembly.hpp"

namespace s3::riding::ground_correction_math {

using math::Vector3;

/// 82D38838..AC: length of Skeleton11008, with two reciprocal-square-root refinements.
float center_of_mass_height(std::array<float, 4> com_to_deck_world);

float dot_product(std::array<float, 4> a, std::array<float, 4> b);

/// 82D38E18..82D38EA8: normalize the force with two refinements, reject
/// lengths no greater than the live native epsilon, then dot with velocity.
/// Velocity deliberately remains unnormalized.
float collision_force_projection(std::array<float, 4> force, std::array<float, 4> velocity);

/// 82D37048: edge direction is divided by its computed length. The native
/// code uses two reciprocal refinements after its two square-root refinements;
/// it does not replace that sequence with direct inverse-length scaling.
std::array<float, 4> edge_direction(std::array<float, 4> start, std::array<float, 4> end);

/// 82D370F8: positive-Y perpendicular to an edge. The degenerate result is
/// native world X (82139A10), not world Y or an arbitrary orthogonal vector.
std::array<float, 4> edge_up(std::array<float, 4> direction);

/// 82D395AC..82D396AC: push away from the obstructing edge and upward.
/// Inputs are Processed1312,1328 and the actual deck position112.
std::array<float, 4> hang_force(std::array<float, 4> edge_start, std::array<float, 4> edge_end,
                                std::array<float, 4> deck);

/// 82D39950..82D399FC: signed deck-Z cross deck-Y, scaled by the native
/// displacement0.007. The caller already checked halfpipe/contact geometry.
std::array<float, 4> wheel_catch_displacement(std::array<float, 4> deck_y, std::array<float, 4> deck_z);

/// 82D39B04..82D39B8C: only captured X/Z are restored. Current position Y
/// appears in both terms, so no vertical pinning correction is introduced.
std::array<float, 4> pinning_velocity(std::array<float, 4> position, float captured_x, float captured_z, float dt);

std::array<float, 4> scale_to_magnitude(std::array<float, 4> vector, float squared, float magnitude);

/// ApplyWorldSpaceForceToDeck82C03F98. Its moment arm is measured from the
/// physical deck part origin returned by82585CB0, not the body's COM origin.
void apply_world_force(physics::assembly::BodySnapshot& body, Vector3 deck_part_position, Vector3 force,
                       Vector3 point);

}  // namespace s3::riding::ground_correction_math
