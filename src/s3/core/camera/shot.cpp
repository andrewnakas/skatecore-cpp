// Port of skate-core/src/camera/shot.rs.
#include "s3/core/camera/shot.hpp"

#include "s3/core/camera/orientation_math.hpp"
#include "s3/core/camera/rig_tracking.hpp"
#include "s3/core/camera/shot_orientation.hpp"
#include "s3/core/camera/vector_tracker.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::camera {

void Shot::update_normal(float north, float heading_mirror) {
    if (this->use_previous_shot != 0) {
        return;
    }
    const float from = this->position_heading;
    const float delta = rig_tracking::wrap_vmx(-this->position_heading - from);
    float heading = rig_tracking::wrap_vmx(rs::mul_add(delta, (1.0f - heading_mirror) * 0.5f, from));
    heading = rig_tracking::wrap_vmx(heading + north);
    this->arm_orientation = orientation_math::quaternion_from_angles(this->position_elevation, heading, 0.0f);
}

F4 Shot::reference_point(const std::array<F4, 10>& positions, F4 board_offset_direction, F4 fallback) const {
    const F4 offset = rs::map(board_offset_direction, [&](float v) { return v * this->board_offset; });
    F4 sum = {0.0f, 0.0f, 0.0f, 0.0f};
    float weight_sum = 0.0f;
    for (std::size_t k = 0; k < 10; ++k) {
        const F4& position = positions[k];
        const float weight = this->reference_weights[k];
        weight_sum = weight + weight_sum;
        sum = rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(position[i] + offset[i], weight, sum[i]); });
    }
    if (rs::from_bits(0x38d1b717) > rs::abs(weight_sum)) {
        return fallback;
    }
    const float inverse = vector_tracker::refined_reciprocal(weight_sum);
    return rs::map(sum, [&](float v) { return inverse * v; });
}

void Shot::interpolate_from(const Shot& from_in, const Shot& to_in, float fraction) {
    // Rust takes both by value (copies): `self` may alias either argument.
    const Shot from = from_in;
    const Shot to = to_in;
    auto blend = [&](float a, float b) { return interpolate_float(a, b, 0, fraction); };
    this->distance = blend(from.distance, to.distance);
    this->lens_length = blend(from.lens_length, to.lens_length);
    this->smoothing = rs::from_fn<4>([&](std::size_t i) { return blend(from.smoothing[i], to.smoothing[i]); });
    this->reference_weights =
        rs::from_fn<10>([&](std::size_t i) { return blend(from.reference_weights[i], to.reference_weights[i]); });
    this->board_offset = blend(from.board_offset, to.board_offset);
    this->position_elevation = blend(from.position_elevation, to.position_elevation);
    this->framing = rs::from_fn<3>([&](std::size_t i) { return blend(from.framing[i], to.framing[i]); });
    this->follow_subject_in_air = to.follow_subject_in_air;
    this->mirror_for_stance = to.mirror_for_stance;
    this->snap_to_reference_point = to.snap_to_reference_point;
    this->use_previous_shot = to.use_previous_shot;
    this->use_drop_predictor = to.use_drop_predictor;
    this->use_free_camera_stick = to.use_free_camera_stick;
    this->avoidance_override = to.avoidance_override;
    this->blur = blend(from.blur, to.blur);
    this->transition_blur = to.transition_blur;
    this->subject_opacity = blend(from.subject_opacity, to.subject_opacity);
    this->collision_hint = to.collision_hint;
    this->anchor = to.anchor;
    this->compass_north = to.compass_north;
    this->world_heading = blend(from.world_heading, to.world_heading);
    this->arm_orientation = shot_orientation::interpolate_orientation(from.arm_orientation, to.arm_orientation, fraction);
    this->camera_orientation =
        shot_orientation::interpolate_orientation(from.camera_orientation, to.camera_orientation, fraction);
}

float interpolate_float(float from, float to, std::uint32_t style, float fraction) {
    switch (style) {
        case 0:
            return rs::mul_add(to - from, shot::sine_blend(fraction), from);
        case 1:
            return to;
        default:
            return from;
    }
}

namespace shot {
float sine_blend(float fraction) {
    const float angle = rs::mul_add(fraction, rs::from_bits(0x40490fdb), -rs::from_bits(0x3fc90fdb));
    return rs::mul_add(trigonometry::sin(angle), 0.5f, 0.5f);
}
}  // namespace shot

float filter_blend_value(float previous, float raw, float smoothing, float minimum, float maximum) {
    const float response = 1.0f - smoothing;
    const float next = rs::mul_add(response * response, raw - previous, previous);
    const float lower = minimum - next >= 0.0f ? minimum : next;
    return maximum - lower >= 0.0f ? lower : maximum;
}

std::optional<std::tuple<std::size_t, std::size_t, float>> blend_interval(std::array<float, 3> points,
                                                                          std::array<bool, 3> present, float value) {
    std::size_t count = 0;
    while (count < 3 && present[count]) {
        ++count;
    }
    if (count == 0) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (points[i] > value) {
            if (i == 0) {
                return std::make_tuple(std::size_t(0), std::size_t(0), 1.0f);
            }
            return std::make_tuple(i - 1, i, (value - points[i - 1]) / (points[i] - points[i - 1]));
        }
    }
    return std::make_tuple(count >= 2 ? count - 2 : std::size_t(0), count - 1, 1.0f);
}

}  // namespace s3::camera
