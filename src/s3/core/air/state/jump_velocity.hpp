// Port of skate-core/src/air/state/jump_velocity.rs.
// TU3 PhysicsAir jump-velocity correction (`0x82D34BC0`).
#pragma once
#include <cstdint>

#include "s3/core/air/state/data.hpp"

namespace s3::air::state::jump_velocity {

using data::PhysicsAirFrame;
using data::Vector4;

inline constexpr float VERTICAL_ERROR_FLOOR = rs::from_bits(0xC0400000u);           // -3.0
inline constexpr float HORIZONTAL_ERROR_LIMIT_SQUARED = rs::from_bits(0x41100000u);  // 9.0

/// Numerical operations used by the recovered formula. The PC adapter uses
/// independently implemented arithmetic and the original refinement steps;
/// agreement with every Xenon floating-point edge case is not established.
/// (Rust trait -> abstract interface.)
class PhysicsAirMath {
public:
    virtual ~PhysicsAirMath() = default;
    /// Native `vminfp128`, used by the OffBoard air-entry velocity cap.
    virtual float minimum_vminfp(float left, float right) = 0;
    /// Native `vmsum3fp128(value, value)`; only lane zero is consumed.
    virtual float length_squared_vmsum3fp(Vector4 value) = 0;
    /// The native vmsum/vrsqrte/two-refinement sequence at 82D34D10..68.
    virtual float length_vmsum3fp_vrsqrte(Vector4 value) = 0;
    /// `PhysicsUtility::ClampVectorWithinMaxLength`, `0x82BD3D90`.
    virtual Vector4 clamp_vector_within_max_length(Vector4 value, float maximum_length) = 0;
};

/// Reconstructs the full branch contract and arithmetic order of `0x82D34BC0`.
/// The frame count is signed in the caller and in the target's `extsw/fcfid`.
Vector4 calculate_velocity_from_jump(const PhysicsAirFrame& frame, std::int32_t frames, PhysicsAirMath& math);

}  // namespace s3::air::state::jump_velocity
