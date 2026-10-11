// Port of skate-core/src/physics/mass.rs.
// Recovered board-part mass properties, separate from integration.
#pragma once
#include <array>
#include <optional>

#include "s3/core/math.hpp"
#include "s3/core/physics/rigid_body.hpp"

namespace s3::physics::mass {

using math::Basis3;
using math::Vector3;
using rigid_body::RetailBodyMassProperties;
using rigid_body::RetailInertiaDynamics;
using rigid_body::RetailLocalMassFrame;

// Private in Rust (visible to the construction/* child modules).
float refined_reciprocal(float value);

}  // namespace s3::physics::mass

#include "s3/core/physics/construction/board_parts.hpp"
#include "s3/core/physics/construction/deck_geometry.hpp"
#include "s3/core/physics/construction/mass_moments.hpp"
#include "s3/core/physics/construction/primitive_mass.hpp"
#include "s3/core/physics/construction/principal_axes.hpp"

namespace s3::physics::mass {

using primitive_mass_mod::MassShape;
using primitive_mass_mod::primitive_mass;
using primitive_mass_mod::PrimitiveMass;
using board_parts::forward_mass_properties;
using board_parts::ForwardMassProperties;
using board_parts::PartMassInput;
using board_parts::truck_mass_input;
using board_parts::TruckMassSettings;
using board_parts::wheel_mass_input;
using board_parts::WheelMassSettings;
using mass_moments::AggregateMassProperties;
using mass_moments::MassMoments;
using deck_geometry::DeckChild;
using deck_geometry::DeckGeometry;
using deck_geometry::DeckGeometrySettings;
using deck_geometry::DeckShape;

/// TU3 ComputeMassProperties82AE7770 for one centered primitive. Skeleton
/// parts use this same finalizer as board parts, including volume fallback
/// when the requested mass is strictly below MIN_POSITIVE; NaN is preserved.
std::optional<RetailBodyMassProperties> primitive_mass_properties(PartMassInput input,
                                                                  float maximum_angular_velocity,
                                                                  float angular_drag);

/// TU3 ComputeMassProperties82AE7770 after compound-volume accumulation.
/// The caller may apply a source-owned part-frame override after this result.
RetailBodyMassProperties aggregate_mass_properties(MassMoments moments, float requested_mass,
                                                   float maximum_angular_velocity,
                                                   float angular_drag);

inline const float RETAIL_UNBOUNDED_VELOCITY = rs::from_bits(0x7F7FFFFFu);
inline const float RETAIL_WHEEL_MAXIMUM_ANGULAR_VELOCITY = rs::from_bits(0x476A5FFFu);

/// Stock wheel properties produced by TU3 `0x82C0AA78 -> 0x82AE7770`.
RetailBodyMassProperties retail_wheel_mass_properties();

RetailBodyMassProperties wheel_mass_properties(WheelMassSettings settings);

/// Stock truck properties produced by TU3 `0x82C0A6F8 -> 0x82AE7770`.
RetailBodyMassProperties retail_truck_mass_properties();

RetailBodyMassProperties truck_mass_properties(TruckMassSettings settings);

/// TU3 deck construction82C09290 -> mass accumulation82AE7058 ->
/// ComputeMassProperties82AE7770. Every authored child contributes to mass.
RetailBodyMassProperties deck_mass_properties(const DeckGeometry& geometry, float mass,
                                              float angular_drag);

/// Stock convenience constructor; the game supplies its loaded XML settings.
RetailBodyMassProperties retail_deck_mass_properties();

/// Body order used by `SkateboardBody`: wheels 0..3, trucks 4..5, deck 6.
std::array<RetailBodyMassProperties, 7> default_skateboard_mass_properties();

}  // namespace s3::physics::mass
