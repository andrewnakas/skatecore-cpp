// Port of skate-core/src/physics/board_animation.rs.
#include "s3/core/physics/board_animation.hpp"

#include "s3/core/math.hpp"
#include "s3/core/physics/constraint_frames.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/skeleton_root.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::board_animation {

using constraint_frames::basis;
using constraint_frames::reciprocal;
using constraint_frames::reciprocal_sqrt;
using drive_frames::retail_quaternion_from_basis;
using math::Basis3;
using native_arithmetic::dot4;
using rigid_body::RetailQuaternion;
using skeleton_animation_record::compose_affine;
using skeleton_animation_record::IDENTITY;
using skeleton_root::inverse_rigid;
using skeleton_root::orthonormalize;

void BoardAnimation::reset() { rotation_error = {0.0f, 0.0f, 0.0f, 1.0f}; }

void BoardAnimation::capture_physics_error(const Transform& target, const Transform& deck_part) {
    const Transform relative = orthonormalize(compose_affine(inverse_rigid(target), deck_part));
    Basis3 b;
    for (std::size_t i = 0; i < 3; ++i) b.columns[i] = {relative[i][0], relative[i][1], relative[i][2]};
    const RetailQuaternion q = retail_quaternion_from_basis(b);
    rotation_error = {q.x, q.y, q.z, q.w};
    blending = true;
}

Transform BoardAnimation::apply(const Transform& target, bool fast, const BoardAnimationSettings& settings) {
    if (!blending) {
        return target;
    }
    const float angle = trigonometry::acos(rotation_error[3]) * 2.0f;
    const float turns = angle * rs::from_bits(0x3e22f983u);
    const float fraction0 = turns - rs::floor(turns);
    const float folded = fraction0 - (fraction0 > 0.5f ? 1.0f : 0.0f);
    const float distance = rs::abs(folded * rs::from_bits(0x40c90fdbu));
    const float fraction = (fast ? settings.fast : settings.slow).evaluate(distance);
    //Original fcmpu/blt: unordered graph output also completes the blend.
    if (!(fraction < rs::from_bits(0x3f733333u))) {
        blending = false;
        return target;
    }
    const std::array<float, 4> identity{0.0f, 0.0f, 0.0f, 1.0f};
    const float dot = dot4(rotation_error, identity);
    const bool reverse = 0.0f > dot;
    const float magnitude = reverse ? -dot : dot;
    std::array<float, 4> q{};
    for (std::size_t i = 0; i < 4; ++i) q[i] = reverse ? -rotation_error[i] : rotation_error[i];
    std::array<float, 4> result{};
    if (magnitude > rs::from_bits(0x3f7f069eu)) {
        const bool same_sign = dot4(q, identity) > 0.0f;
        std::array<float, 4> mixed{};
        for (std::size_t lane = 0; lane < 4; ++lane) {
            mixed[lane] = same_sign ? rs::mul_add(fraction, identity[lane] - q[lane], q[lane])
                                    : rs::mul_add(-(identity[lane] + q[lane]), fraction, q[lane]);
        }
        const float inverse = reciprocal_sqrt(dot4(mixed, mixed));
        for (std::size_t i = 0; i < 4; ++i) result[i] = mixed[i] * inverse;
    } else {
        const float theta = trigonometry::acos(magnitude);
        const float a = trigonometry::sin((1.0f - fraction) * theta);
        const float b = trigonometry::sin(fraction * theta);
        const float inverse = reciprocal(trigonometry::sin(theta));
        const float weight_a = a * inverse;
        const float weight_b = b * inverse;
        for (std::size_t lane = 0; lane < 4; ++lane) {
            result[lane] = rs::mul_add(identity[lane], weight_b, q[lane] * weight_a);
        }
    }
    rotation_error = result;
    //82C01B04..BE0 uses the same sqrt(2) / paired residual basis kernel
    //independently present in82AE1CC4. No quaternion normalization here.
    const Basis3 rotation = basis(RetailQuaternion{result[0], result[1], result[2], result[3]});
    Transform correction = IDENTITY;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        for (std::size_t j = 0; j < 3; ++j) correction[axis][j] = rotation.columns[axis][j];
    }
    return compose_affine(target, correction);
}

std::array<float, 4> target_velocity(std::array<float, 4> target, std::array<float, 4> deck_body_position, float dt) {
    const float inverse = 1.0f / dt;
    std::array<float, 4> out{};
    for (std::size_t lane = 0; lane < 4; ++lane) out[lane] = (target[lane] - deck_body_position[lane]) * inverse;
    return out;
}

}  // namespace s3::physics::board_animation
