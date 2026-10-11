// Port of skate-game/src/physics/footplant/pose.rs.
//! Footplant82D70070 publishes real FootIK targets and temporary collision disables.
#include "s3/game/physics/footplant/pose.hpp"

#include "s3/core/trigonometry.hpp"
#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/foot_ik.hpp"

namespace s3::game::physics::footplant {

using namespace ::s3::game::physics::footplant::math;

void Footplant::update_pose(const sc::air::known::KnownAirFootplantInput& input,
                            const animated_skeleton::AnimatedSkeleton& skeleton, foot_ik::FootIk& ik,
                            sc::physics::skeleton_body::SkeletonCollisionMode& collision) {
    //82453298, shared source-derived polynomial and independent PC estimate seed.
    const float angle = sc::trigonometry::acos(dot(launch_direction, leg_direction));
    const float turns = angle * rs::from_bits(0x3e22f983u);
    const float fraction = turns - rs::floor(turns);
    const float wrapped = (fraction - (fraction > 0.5f ? 1.0f : 0.0f)) * rs::from_bits(0x40c90fdbu);
    if (requested && candidate && contact_time < 0.3f) {
        const float contact_length = length(sub(adjusted_contact, animation_com));
        const float leg_length = length(sub(selected_world, animation_com));
        const float distance = leg_length < contact_length ? leg_length : contact_length;
        const auto& inverse = skeleton.roots.world_to_animation;
        const V com = point(inverse, animation_com);
        const V velocity = rotate(inverse, scale(input.trajectory.velocity, -STEP));
        const V direction = rotate(inverse, launch_direction);
        V target = madd(direction, distance, add(com, velocity));
        if (lock_valid) {
            target = add(locked_target, clamp_length(sub(target, locked_target), 0.15f));
        }
        locked_target = target;
        lock_valid = true;
        const std::size_t side = selected_toe == std::optional<std::size_t>(15) ? 0 : 1;
        ik.state.external_targets[side].animation_position = target;
        ik.state.limbs[side].local_target_set = true;
        ik.state.limbs[side].target_blend = target_blend;
        target_blend = clamp01(target_blend + 0.12f);
        if (contact_time < 0.2f) {
            const std::array<std::size_t, 3> parts =
                side == 0 ? std::array<std::size_t, 3>{15, 16, 17} : std::array<std::size_t, 3>{19, 20, 21};
            for (const std::size_t part : parts) {
                collision.pending_reenable = true;
                collision.disable_count[part] = 2;
                collision.parts[part].enabled = false;
            }
        }
    }
    if (requested) {
        perform = hit && !(contact_time > 0.017f) &&
                  settings.max_leg_angle_error * rs::from_bits(0x3c8efa35u) > wrapped;
    } else {
        target_blend = 0.0f;
        perform = false;
    }
}

}  // namespace s3::game::physics::footplant
