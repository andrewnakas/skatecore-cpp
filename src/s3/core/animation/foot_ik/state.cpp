// Port of skate-core/src/animation/foot_ik/state.rs.
#include "s3/core/animation/foot_ik/state.hpp"

#include "s3/core/animation/foot_ik/blend.hpp"

namespace s3::animation::foot_ik::state {

using status::Mode;

std::array<bool, 4> FootIkState::update(const UpdateInput& input, const Geometry& geometry,
                                        const Settings& settings, std::array<Transform, 24>& drives) {
    for (std::size_t limb = 0; limb < 4; ++limb) {
        frames[limb].target = input.targets[limb];
    }
    status::update_modes(limbs, feet_enabled_, input.flags_2472);
    status::update_blends(limbs, settings.deck_half_width, settings.deck_total_half_length, input.flags_2480,
                          settings.blend);
    const Transform inverse_animation_board = transforms::inverse_rigid(input.animation[0]);
    for (std::size_t limb = 0; limb < 4; ++limb) {
        frames[limb].within_contact_bounds = false;
        if (limbs[limb].mode == Mode::External || limbs[limb].mode == Mode::Local) {
            external::update(external_targets[limb], limbs[limb], frames[limb], LIMBS[limb], input.animation,
                             input.animation_to_world, input.world_to_animation);
        }
        transforms::prepare_animation_target(limbs[limb], frames[limb], LIMBS[limb], input.animation,
                                             input.animation_to_world, inverse_animation_board,
                                             settings.contact_bounds);
    }
    blend::update(limbs, frames, LIMBS, input.physical_board, input.animation_to_world, input.animation[0]);
    contacts.update(limbs, frames,
                    contact::ContactInput{
                        input.flags_2468,
                        input.contact_bone,
                        input.foot_bones,
                        input.hips_world_position,
                        input.inverse_contact_board,
                        input.contact_board,
                        input.current_contacts,
                    });
    return drive::update(limbs, frames, LIMBS, geometry, input.original_animation, drives,
                         input.world_to_animation, settings.angle_limits);
}

}  // namespace s3::animation::foot_ik::state
