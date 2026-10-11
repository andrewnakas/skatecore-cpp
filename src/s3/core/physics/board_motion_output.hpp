// Port of skate-core/src/physics/board_motion_output.rs.
// Observed deck motion from TU3 Skateboard::FillPhysOut82C02A80.
// These calculations consume the live solver body and the actual Reckoning
// normal. They do not infer contact state from height or vertical velocity.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::board_runtime {
class BoardRuntime;
}

namespace s3::physics::board_motion_output {

using math::Basis3;
using math::Vector3;

struct BoardMotionOutput {
    Vector3 angular_velocity;
    Vector3 linear_velocity;
    Vector3 ground_velocity;
    /// SkateboardMotion+160: magnitude of the unprojected deck velocity.
    float speed = 0.0f;
    /// SkateboardMotion+164: magnitude after rejecting the Reckoning normal.
    float ground_speed = 0.0f;
    /// SkateboardMotion+168: raw deck velocity projected onto effective Z.
    float forward_speed = 0.0f;
    Basis3 effective_basis;

    static BoardMotionOutput from_board(const board_runtime::BoardRuntime& board, Vector3 reckoning_ground_normal,
                                        std::uint32_t processed_flags_2468);
};
S3_DERIVE_EQ(BoardMotionOutput, angular_velocity, linear_velocity, ground_velocity, speed, ground_speed,
             forward_speed, effective_basis)

inline float dot(Vector3 a, Vector3 b) {
    return native_arithmetic::dot3({a.x, a.y, a.z, 0.0f}, {b.x, b.y, b.z, 0.0f});
}
/// pub(crate)
inline Vector3 scale(Vector3 v, float s) { return Vector3{v.x * s, v.y * s, v.z * s}; }
/// pub(crate)
inline Vector3 add(Vector3 a, Vector3 b) { return Vector3{a.x + b.x, a.y + b.y, a.z + b.z}; }
/// pub(crate)
inline Vector3 subtract(Vector3 a, Vector3 b) { return Vector3{a.x - b.x, a.y - b.y, a.z - b.z}; }
/// pub(crate)
inline float inverse_length_squared(float squared, std::size_t refinements) {
    float inverse = native_arithmetic::reciprocal_square_root_estimate(squared);
    for (std::size_t i = 0; i < refinements; ++i) {
        const float correction = rs::mul_add(-squared, inverse * inverse, 1.0f);
        inverse = rs::mul_add(inverse * 0.5f, correction, inverse);
    }
    return inverse;
}
/// pub(crate). Normalise one Gram-Schmidt row, keeping `fallback` when it is degenerate.
///
/// [`inverse_length_squared`] refines an `frsqrte` estimate, and that estimate is infinite for a
/// zero-length row: the first Newton step is `fma(-0, inf, 1)` = NaN, so the row -- and then the
/// whole affine, because `compose_affine`'s translation mixes every rotation row -- becomes NaN.
/// Retail never reaches exactly zero here; this port does, because the board offset's rows lerp
/// *linearly* toward identity, so a ~180-degree offset crosses an exactly-zero row mid-decay. That
/// is the landing crash: `animation_to_world` went NaN, `lifted_com_frame[3]` with it, and the
/// skeleton's lifted-COM target body took a NaN velocity into the solver.
///
/// Only an exactly-degenerate or non-finite row is touched, so a row retail could produce keeps
/// the original arithmetic bit for bit.
inline std::array<float, 4> normalize_row_or(std::array<float, 4> vector, std::array<float, 4> fallback) {
    const float squared = native_arithmetic::dot3(vector, vector);
    if (squared <= 0.0f || !rs::is_finite(squared)) {
        return fallback;
    }
    const float reciprocal = inverse_length_squared(squared, 2);
    if (!rs::is_finite(reciprocal)) {
        return fallback;
    }
    return {vector[0] * reciprocal, vector[1] * reciprocal, vector[2] * reciprocal, vector[3] * reciprocal};
}

/// Vector length, with the source's degenerate guard given the reach it has on the hardware.
///
/// Retail tests `squared == 0.0` and that is enough on Xenon: VMX runs in non-Java mode, where a
/// denormal result flushes to zero, so every vector shorter than ~1.1e-19 reaches the test as an
/// exact zero. Here `dot` keeps the denormal, so the test misses and [`inverse_length_squared`]
/// runs on it -- and its Newton step squares an estimate of ~1e19, which overflows f32 to
/// infinity and settles on `inf - inf` = NaN. So `length` returned NaN for any squared length
/// below ~2.9e-39.
///
/// That is the landing crash. `up_velocity` decays geometrically through `anti_wobble_damping`
/// and `extra_side_damping` as the skater settles after a touchdown; once a lane reaches ~1e-20,
/// `clamp_length`'s `magnitude < ...` test is false against NaN, so it skips its own early return
/// and scales by a NaN reciprocal. That NaN becomes `up_velocity`, then `up`, and
/// `calculate_transform` writes it straight into `system[1]` -- the NaN up row the owner's crash
/// log names, with rows 0 and 2 still finite because their Gram-Schmidt guards fell back.
///
/// Flushing the denormal restores the original guard's reach. Every squared length the hardware
/// would have kept normal is untouched, so a non-degenerate vector keeps its exact former value.
inline float length(Vector3 v) {
    const float squared = dot(v, v);
    if (squared < rs::f32::MIN_POSITIVE) {
        return 0.0f;
    }
    return squared * inverse_length_squared(squared, 2);
}

}  // namespace s3::physics::board_motion_output
