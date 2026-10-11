// Port of skate-core/src/camera/avoidance.rs.
#include "s3/core/camera/avoidance.hpp"

#include "s3/core/camera/direction.hpp"
#include "s3/core/camera/rig_tracking.hpp"
#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera {
namespace {

float floor_zero(float v) { return v >= 0.0f ? v : 0.0f; }
float clamp(float v) {
    v = -v >= 0.0f ? 0.0f : v;
    return 1.0f - v >= 0.0f ? v : 1.0f;
}

}  // namespace

using vector_tracker::dot;
using vector_tracker::length;
using vector_tracker::refined_reciprocal;

void RigAvoidance::update(float dt, AvoidanceSettings settings, const Positioner& positioner, F4 last_breadcrumb,
                          AvoidanceSubject& subject, std::array<TrajectoryCollisionRequest*, 3>& requests,
                          MovingObstacleProvider& moving) {
    if (this->avoidance_mode == 1 && this->subject_speed(subject) < 0.5f) {
        return;
    }
    if ((this->flags_516 & 2) != 0 || this->disable_time > 0.0f || this->avoidance_mode == 0) {
        this->heading_weight = 0.0f;
        this->elevation_weight = 0.0f;
        return;
    }
    switch (this->avoidance_mode) {
        case 1: {
            F4 anchor_ = this->tracked_anchor;
            anchor_[1] = this->reference_height;
            const F4 delta = rs::from_fn<4>([&](std::size_t i) { return last_breadcrumb[i] - anchor_[i]; });
            const float magnitude = length(delta);
            if (rs::from_bits(0x38d1b717) > magnitude) {
                return;
            }
            const float inverse = refined_reciprocal(magnitude);
            const F4 angles = direction_to_angles(rs::map(delta, [&](float v) { return inverse * v; }));
            this->avoidance_heading = -angles[1];
            this->avoidance_elevation = angles[0];
            break;
        }
        case 2: {
            const F4 angles = direction_to_angles(subject.vector_452());
            this->avoidance_heading = -angles[1];
            this->avoidance_elevation = angles[0];
            break;
        }
        case 3:
            this->avoidance_heading = this->heading_reference;
            this->avoidance_elevation = rs::from_bits(0x3fbde44e);
            break;
        case 4: {
            this->avoidance_heading = rig_tracking::wrap_vmx(this->heading_target + rs::from_bits(0x40490fdb));
            const float low = rs::from_bits(0x3e32b8c2);
            this->avoidance_elevation = low - this->elevation_target >= 0.0f ? low : this->elevation_target;
            break;
        }
        default:
            break;
    }
    if (rs::is_nan(this->avoidance_elevation)) {
        return;
    }
    const F4 prediction = position_from_angles(this->anchor, F4{}, this->avoidance_elevation,
                                               this->avoidance_heading, this->prediction_distance,
                                               positioner.config.clamp_height,
                                               positioner.radius + positioner.config.floor_height);
    const bool transitioning = this->height_transition_time < this->height_transition_duration;
    F4 velocity_ = this->velocity;
    if (transitioning) {
        velocity_[1] =
            (this->height_transition_end - this->height_transition_start) / this->height_transition_duration;
    }
    const std::uint8_t acceleration_flag = (this->height_mode == 1 && !transitioning) ? 1 : 0;
    this->paths[0].set(
        PredictionPath{
            // v127 preserves the current positioner position across the
            // prediction helper; its returned point is used by path1.
            positioner.position,
            velocity_,
            settings.radius_padding + positioner.radius,
            settings.horizon,
        },
        acceleration_flag);
    std::uint32_t context = subject.context_532();
    this->paths[0].evaluator.update(context, *requests[0], moving);
    const float camera_time = this->paths[0].evaluator.last_valid_time;
    const F4 second_start = this->height_mode == 1
                                ? prediction
                                : F4{this->anchor[0], positioner.position[1], this->anchor[2], this->anchor[0]};
    this->paths[1].set(PredictionPath{second_start, velocity_, rs::from_bits(0x3ca3d70a), settings.horizon},
                       acceleration_flag);
    context = subject.context_532();
    this->paths[1].evaluator.update(context, *requests[1], moving);
    const F4 third_start = subject.vector_444();
    this->paths[2].set(PredictionPath{third_start, velocity_, rs::from_bits(0x3ca3d70a), settings.horizon},
                       acceleration_flag);
    context = subject.context_532();
    this->paths[2].evaluator.update(context, *requests[2], moving);
    const float second_time = this->paths[1].evaluator.last_valid_time;
    const float third_time = this->paths[2].evaluator.last_valid_time;
    const bool any_acceleration = this->paths[1].evaluator.result_acceleration_flag != 0 ||
                                  this->paths[2].evaluator.result_acceleration_flag != 0;
    const float other_time = second_time - third_time >= 0.0f ? second_time : third_time;
    if (third_time >= second_time) {
        this->flags_516 |= 1;
    }
    const std::uint8_t subject_flag = subject.flag_608();
    if (subject_flag == 0 && (positioner.flags & 0x20) != 0) {
        this->heading_weight = dt * 0.5f + this->heading_weight;
        this->elevation_weight = dt * 0.5f + this->elevation_weight;
    } else if (subject_flag == 0 && this->height_mode != 1 &&
               (any_acceleration ? other_time * 0.5f > camera_time : other_time > camera_time)) {
        const float target = clamp(1.0f - camera_time / settings.horizon);
        const float heading_target_ = this->heading_weight - target >= 0.0f ? this->heading_weight : target;
        const float elevation_target_ = this->elevation_weight - target >= 0.0f ? this->elevation_weight : target;
        this->heading_weight = rs::mul_add(1.0f - settings.heading_smoothing, heading_target_ - this->heading_weight,
                                           this->heading_weight);
        this->elevation_weight = rs::mul_add(1.0f - settings.elevation_smoothing,
                                             elevation_target_ - this->elevation_weight, this->elevation_weight);
    } else if ((this->subject_speed(subject) > rs::from_bits(0x3e19999a) && this->reset_time <= 0.0f) ||
               subject.flag_548() != 0) {
        this->heading_weight = floor_zero(this->heading_weight - dt / settings.ease_out_time);
        this->elevation_weight = floor_zero(this->elevation_weight - dt / settings.ease_out_time);
        this->flags_516 &= std::uint8_t(~1u);
    }
    this->heading_weight = clamp(this->heading_weight);
    this->elevation_weight = clamp(this->elevation_weight);
}

float RigAvoidance::subject_speed(AvoidanceSubject& subject) const {
    return dot(this->velocity, subject.transform_376()[2]);
}

void AvoidancePath::set(PredictionPath path, std::uint8_t acceleration_flag) {
    this->evaluator.path = path;
    this->prediction_flag = 0;
    this->evaluator.acceleration_flag = acceleration_flag;
}

}  // namespace s3::camera
