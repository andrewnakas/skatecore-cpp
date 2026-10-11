// Port of skate-core/src/player/offboard/ground_reckoning.rs (whole file).
#include "s3/core/player/offboard/ground_reckoning.hpp"

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/player/offboard/contact_correction.hpp"

namespace s3::player::offboard::ground_reckoning {

Output update(riding::ground_orientation::GroundOrientation& orientation,
              riding::reckoning_frames::ReckoningFrames& frames, air::body_spin::BodySpinState& body_spin,
              air::reckoning::data::AirState& air, const Settings& settings, const Input& input) {
    orientation.dynamic_up = xyz(input.previous_up);
    frames.target_lean_angle = 0.0f;
    air.secondary_lean_angle = 0.0f;
    orientation.up_velocity = s3::math::Vector3::ZERO;
    const Vector ground = orientation.ground_filter.update(settings.ground_normal_smoothing, input.requested_up);
    orientation.ground_normal = xyz(ground);
    frames.heading = input.requested_forward;
    const float retained = 1.0f - input.blend;
    const Vector candidate = rs::from_fn<4>([&](std::size_t i) {
        return rs::mul_add(input.previous_up[i], input.blend, input.requested_up[i] * retained);
    });
    const float square = physics::native_arithmetic::dot3(candidate, candidate);
    const float inverse = contact_correction::inverse_length(square);
    const float length = square == 0.0f ? 0.0f : square * inverse;
    const Vector up = length > rs::from_bits(0x358637bdu) ? rs::map(candidate, [&](float v) { return v * inverse; })
                                                          : input.previous_up;
    orientation.up = xyz(up);
    orientation.target = xyz(up);
    orientation.slow_filter.filter(up);
    orientation.fast_filter.filter(up);
    orientation.slow_filter.publish_current(up);
    orientation.fast_filter.publish_current(up);
    frames.calculate_transform(up, ground);
    frames.calculate_tilt(input.reverse_stance, *settings.tilt_vs_rotation, *settings.tilt_vs_slope);
    air::body_spin::update_ground(body_spin, input.enable_body_spin_input ? input.physical_body_spin_2812 : 0.0f);
    return Output{input.previous_up, up, up, Vector{}, ground};
}

}  // namespace s3::player::offboard::ground_reckoning
