// Port of skate-core/src/camera/manager_blends.rs (pub(super)).
//! Complete normal-shot blend callback82E07708 and air blends82DFE898/E998.
#pragma once
#include <cstdint>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/manager_state.hpp"
#include "s3/core/camera/manager_subject.hpp"
#include "s3/core/camera/rig.hpp"
#include "s3/core/camera/shot_manager.hpp"

namespace s3::camera::manager_blends {

struct BlendEnvironment : ShotEnvironment {
    const ManagerState* state = nullptr;
    const ManagerSubject* subject = nullptr;
    const Rig* rig = nullptr;
    F4 rig_forward{};
    F4 angular_velocity{};
    /// Rig424, including the compass2 override from the previous binding.
    float heading_multiplier = 0.0f;
    /// ShotManager992+60: previous shot distance.
    float previous_distance = 0.0f;

    BlendEnvironment(const ManagerState& state_, const ManagerSubject& subject_, const Rig& rig_, F4 rig_forward_,
                     F4 angular_velocity_, float heading_multiplier_, float previous_distance_)
        : state(&state_),
          subject(&subject_),
          rig(&rig_),
          rig_forward(rig_forward_),
          angular_velocity(angular_velocity_),
          heading_multiplier(heading_multiplier_),
          previous_distance(previous_distance_) {}

    float heading_mirror() const override { return this->heading_multiplier; }
    float compass_north(std::uint32_t entry) const override { return this->subject->compass[entry]; }
    bool special_camera_flag() const override { return (this->state->flags & 0x10) != 0; }
    float raw_blend_value(std::uint32_t kind, float authored, float dt) override;

private:
    float air_progress() const;
    float apex_progress() const;
};

}  // namespace s3::camera::manager_blends
