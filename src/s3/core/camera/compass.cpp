// Port of skate-core/src/camera/compass.rs.
#include "s3/core/camera/compass.hpp"

#include "s3/core/camera/direction.hpp"
#include "s3/core/camera/manager_incline.hpp"
#include "s3/core/camera/manager_state.hpp"
#include "s3/core/camera/manager_subject.hpp"
#include "s3/core/camera/orientation_math.hpp"
#include "s3/core/camera/rig_tracking.hpp"
#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera {

using compass::blend;
using compass::heading;
using compass::horizontal;
using compass::PI;
using compass::sub;
using compass::TAU;
using compass::wrap;
using compass::wrap_floor;
using manager_state::clamp;
using vector_tracker::length;

std::array<float, 9> Compass::update(float dt, const CompassInputs& input, const CompassSettings& settings) {
    if (this->previous_reset) {
        this->previous_position = input.transform[3];
        this->stopped_time = 0.0f;
        this->deadzone_size = 0.0f;
    }
    this->update_movement(dt, input, settings);
    float launch = heading(input.ground_normal);
    float landing = heading(input.landing_normal);
    if (rs::abs(launch - landing) > PI) {
        if (launch >= landing) {
            landing += TAU;
        } else {
            launch += TAU;
        }
    }
    this->headings[1] = wrap(rs::mul_add(landing - launch, input.trajectory_fraction, launch));
    this->headings[0] = this->movement_heading;
    const F4 difference = horizontal(sub(input.landing_position, input.launch_position));
    if (input.trajectory_fraction > 0.0f && length(difference) > 1.0f) {
        this->headings[4] = dt == 0.0f ? direction_to_angles(input.skeleton_direction)[1]
                                       : -direction_to_angles(input.trajectory_direction)[1];
        this->headings[0] = wrap(blend(this->headings[0], this->headings[4], input.trajectory_fraction));
    }
    this->headings[3] = heading(input.transform[2]);
    this->update_orbit(dt, input);
    this->update_follow(input);
    const F4 direction = horizontal(sub(input.transform[3], input.camera_position));
    const float forward = heading(input.transform[2]);
    const float camera_heading = heading(direction);
    const float delta = wrap_floor(camera_heading - forward);
    const float minimum = rs::from_bits(0x3f9c61aa);
    if (rs::abs(delta) <= minimum) {
        this->headings[8] = delta >= 0.0f ? forward + minimum : forward - minimum;
    } else {
        this->headings[8] = camera_heading;
    }
    this->previous_reset = dt == 0.0f;
    return this->headings;
}

void Compass::update_follow(const CompassInputs& input) {
    const F4 relative = sub(input.transform[3], input.camera_position);
    if (input.selected_compass != 7) {
        this->headings[7] = heading(horizontal(relative));
    } else if (input.air_flag_452) {
        this->headings[7] = heading(horizontal(sub(input.look_target, input.transform[3])));
    } else {
        const F4 velocity_ = horizontal(this->velocity);
        const float speed = length(velocity_);
        if (speed > 0.7f) {
            const float maximum = clamp((speed - 0.7f) * rs::from_bits(0x3ede9bd3), 0.0f, 1.0f) * 0.6f;
            const float target = heading(velocity_);
            const float previous = heading(relative);
            this->headings[7] = clamp(wrap_floor(target - previous) * 0.25f, -maximum, maximum) + previous;
        }
    }
}

namespace compass {
float wrap(float v) { return rig_tracking::wrap_vmx(v); }
float blend(float from, float to, float weight) { return wrap(rs::mul_add(wrap(to - from), weight, from)); }
float wrap_floor(float v) {
    const float turns = rs::floor(rs::mul_add(v, rs::from_bits(0x3e22f983), 0.5f));
    return -rs::mul_add(turns, TAU, -v);
}
float heading(F4 v) {
    if (v[1] > rs::from_bits(0x3f7ff2e5)) {
        return 0.0f;
    }
    return manager_incline::atan_ratio(-v[0], v[2]);
}
}  // namespace compass

CompassInputs CompassPoseInputs::bind(const ManagerSubject& subject, F4 camera_position,
                                      std::uint32_t selected_compass) const {
    const bool trajectory = subject.rig.trajectory_valid != 0;
    F4 trajectory_direction_;
    if (subject.rig.grinding != 0) {
        trajectory_direction_ = subject.direction_424;
    } else if (trajectory) {
        trajectory_direction_ = this->trajectory_direction;
    } else {
        trajectory_direction_ = orientation_math::normalize(this->board_velocity);
    }
    CompassInputs out;
    out.ground_normal = subject.rig.last_valid_ground_up;
    out.landing_normal = trajectory ? subject.landing_normal : subject.ground_normal;
    // 82DF80D8 chooses cache80 before 82DF69C0 publishes setter24.
    // 82DF7B90 copies that same cache into compass32..80. Setter28
    // updates the separate physical history, not this orbit centre.
    out.transform = subject.rig.transform;
    out.trajectory_direction = trajectory_direction_;
    out.launch_position = trajectory ? subject.launch_position : F4{};
    out.landing_position = trajectory ? subject.landing_position : F4{};
    out.grind_direction = subject.direction_424;
    out.skeleton_direction = this->skeleton_direction;
    out.camera_position = camera_position;
    out.look_target = this->look_target;
    out.look = subject.look;
    out.trajectory_fraction =
        trajectory ? clamp(subject.trajectory_time / subject.trajectory_duration, 0.0f, 1.0f) : 0.0f;
    out.selected_compass = selected_compass;
    out.no_trajectory = !trajectory;
    out.state_103 = this->state_103;
    out.wiping_out = subject.rig.wiping_out != 0;
    out.grinding = subject.rig.grinding != 0;
    out.air_flag_452 = subject.rig.air_flag_452 != 0;
    return out;
}

}  // namespace s3::camera
