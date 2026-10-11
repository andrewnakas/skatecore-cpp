// Port of skate-core/src/camera/frame_composer.rs.
#include "s3/core/camera/frame_composer.hpp"

#include "s3/core/camera/orientation_math.hpp"
#include "s3/core/camera/shot_orientation.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::camera {
namespace {

using math::Basis3;

/// The signed diagonal candidates and strict component selection are the
/// inline matrix conversion82E01608..82E01730; signs are XOR negations.
F4 quaternion_from_basis(const Basis3& basis) {
    const auto& right = basis.columns[0];
    const auto& up = basis.columns[1];
    const auto& at = basis.columns[2];
    const float x = right[0];
    const float y = up[1];
    const float z = at[2];
    const float trace = (x + y) + z;
    float sum;
    F4 companion;
    std::size_t dominant;
    if (trace > 0.0f) {
        sum = (x + y) + (z + 1.0f);
        companion = {up[2] - at[1], at[0] - right[2], right[1] - up[0], 0.5f};
        dominant = 3;
    } else if (x > y && x > z) {
        sum = (x + (-y)) + ((-z) + 1.0f);
        companion = {0.5f, up[0] + right[1], at[0] + right[2], up[2] - at[1]};
        dominant = 0;
    } else if (y > z) {
        sum = ((-x) + y) + ((-z) + 1.0f);
        companion = {up[0] + right[1], 0.5f, at[1] + up[2], at[0] - right[2]};
        dominant = 1;
    } else {
        sum = ((-x) + (-y)) + (z + 1.0f);
        companion = {at[0] + right[2], at[1] + up[2], 0.5f, right[1] - up[0]};
        dominant = 2;
    }
    float inverse = physics::reciprocal_sqrt::estimate(sum);
    for (int k = 0; k < 2; ++k) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-sum, inverse * inverse, 1.0f), inverse);
    }
    const float root = sum * inverse;
    const float half_inverse = 0.5f * inverse;
    return rs::from_fn<4>([&](std::size_t i) { return companion[i] * (i == dominant ? root : half_inverse); });
}

}  // namespace

F4 FrameComposer::compose(float dt, float fraction, float mirror, const Shot& from, const Shot& to,
                          F4 fallback_reference, F4 camera_position, const FrameSubject& subject,
                          FrameSettings settings) {
    const F4 to_orientation = this->single(to, fallback_reference, camera_position, mirror, subject, settings);
    const F4 from_orientation = this->single(from, fallback_reference, camera_position, mirror, subject, settings);
    const F4 orientation = shot_orientation::slerp(from_orientation, to_orientation, fraction);
    this->roll = dt > 0.0f ? rs::mul_add((to.framing[0] - this->roll) * settings.roll_response, dt, this->roll)
                           : to.framing[0];
    return orientation;
}

F4 FrameComposer::single(const Shot& shot, F4 fallback_reference, F4 camera_position, float mirror,
                         const FrameSubject& subject, FrameSettings settings) {
    this->reference = shot.reference_point(subject.positions, subject.board_offset_direction, fallback_reference);
    this->yaw = shot.framing[1] * mirror;
    this->pitch = -shot.framing[2];
    const Basis3 framing = orientation_math::basis_from_angles(this->pitch, this->yaw, 0.0f);
    std::array<float, 3> direction =
        rs::from_fn<3>([&](std::size_t i) { return this->reference[i] - camera_position[i]; });
    // Native copies X into W for the comparison, so only XYZ can cause
    // the nonzero path. The retained fourth position lane is not tested.
    const float tiny = rs::from_bits(0x34000000);
    if (!(rs::abs(direction[0]) > tiny || rs::abs(direction[1]) > tiny || rs::abs(direction[2]) > tiny)) {
        direction = {0.0f, 0.0f, 1.0f};
    } else {
        direction = orientation_math::normalize(direction);
        const float ceiling = trigonometry::sin(settings.maximum_pitch_degrees * rs::from_bits(0x3c8efa35));
        if (direction[1] > ceiling) {
            direction[1] = ceiling;
            direction = orientation_math::normalize(direction);
        }
    }
    const Basis3 look = orientation_math::look_basis(direction);
    Basis3 result;
    result.columns = rs::map(framing.columns, [&](const std::array<float, 3>& column) {
        return rs::from_fn<3>([&](std::size_t i) {
            return rs::mul_add(look.columns[2][i], column[2],
                               rs::mul_add(look.columns[1][i], column[1], look.columns[0][i] * column[0]));
        });
    });
    return quaternion_from_basis(result);
}

}  // namespace s3::camera
