// Port of skate-core/src/camera/manager_subject.rs.
#include "s3/core/camera/manager_subject.hpp"

#include "s3/core/camera/direction.hpp"
#include "s3/core/camera/orientation_math.hpp"

namespace s3::camera {
namespace {

// Vector comparisons distinguish zero from positive, unlike signum().
int sign(float value) {
    if (value >= 0.0f) {
        return value > 0.0f ? 1 : 0;
    }
    return -1;
}

}  // namespace

bool ManagerSubject::is_ground_camera(std::uint32_t height_mode) const {
    return height_mode == 2 || (this->flag_556 == 0 && this->rig.trajectory_valid == 0 &&
                                this->trajectory_duration <= rs::from_bits(0x3ecccccd));
}

float ManagerSubject::steering_for_turn(bool mirrored) const {
    const float primary = this->steering[0];
    const float first = this->steering[2];
    const float second = this->steering[3];
    float selected = rs::abs(second) > rs::abs(first) ? second : first;
    if (sign(selected) != sign(primary) || rs::abs(selected) < rs::abs(primary)) {
        selected = primary;
    }
    return std::uint8_t(mirrored ? 1 : 0) == this->stance_560 ? selected : -selected;
}

float ManagerSubject::steering_for_blend(bool mirrored) const {
    const float primary = this->steering[1];
    const float first = this->steering[2];
    const float second = this->steering[3];
    const float secondary = rs::abs(second) > rs::abs(first) ? second : first;
    const float selected =
        (sign(primary) != sign(secondary) || rs::abs(primary) < rs::abs(secondary)) ? secondary : primary;
    return std::uint8_t(mirrored ? 1 : 0) == this->stance_560 ? selected : -selected;
}

float ManagerSubject::valid_trajectory_duration() const {
    return this->rig.trajectory_valid != 0 ? this->trajectory_duration : 0.0f;
}

float ManagerSubject::look_heading() const {
    const F4 direction = orientation_math::normalize(F4{this->look[0], 0.0f, this->look[1], 0.0f});
    return -direction_to_angles(direction)[1];
}

}  // namespace s3::camera
