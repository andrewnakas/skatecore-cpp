// Port of skate-core/src/animation/foot_ik/post_physics.rs.
#include "s3/core/animation/foot_ik/post_physics.hpp"

#include "s3/core/animation/foot_ik/physical_solve.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::animation::foot_ik::post_physics {

using physics::native_arithmetic::dot3;
using physics::skeleton_animation_record::compose_affine;
using physics::skeleton_body::SkeletonBody;
using state::FootIkState;
using state::LIMBS;

namespace {

std::array<float, 4> sub(std::array<float, 4> a, std::array<float, 4> b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]};
}

void feet(FootIkState& state, SkeletonBody& body, const drive::Geometry& geometry, const settings::Settings& settings,
          const post_contact::SettingsPost& post, const Input& input, std::array<bool, 4>& updated) {
    Transform effective_board = input.board;
    if ((input.flags_2484 & (1u << 21)) != 0) {
        for (auto& v : effective_board[0]) v = v * -1.0f;
        for (auto& v : effective_board[1]) v = v * -1.0f;
    }
    const std::array<float, 4> up = effective_board[1];
    float up_y = up[0] * input.world_to_animation[0][1];
    up_y = rs::mul_add(up[1], input.world_to_animation[1][1], up_y);
    up_y = rs::mul_add(up[2], input.world_to_animation[2][1], up_y);
    const bool constrained = (input.flags_2468 & (1u << 18)) != 0;
    if ((input.flags_2484 & (1u << 20)) != 0 || post.minimum_board_up > up_y ||
        (constrained && input.value_2664 > rs::from_bits(0x3D23D70Au)) ||
        (constrained && input.state_2520 == 500)) {
        return;
    }
    for (std::size_t limb = 0; limb < 2; ++limb) {
        const auto binding = LIMBS[limb];
        const std::size_t adjacent = binding.parent_part.value();
        if (state.frames[limb].within_contact_bounds && !input.wipeout) {
            state.frames[limb].world = compose_affine(input.board, state.frames[limb].board);
            state.frames[limb].parent_world = compose_affine(input.board, state.frames[limb].parent_board);
            const auto offset = sub(state.frames[limb].parent_world[3], body.record.pose[adjacent][3]);
            if (physical_solve::solve(body, geometry, adjacent, offset)) {
                body.set_part_transform(binding.part, state.frames[limb].world);
                body.set_part_transform(adjacent, state.frames[limb].parent_world);
                updated[limb] = true;
            }
        } else if (!state.limbs[limb].external_target_set) {
            Transform foot = body.record.pose[binding.part];
            if (dot3(foot[1], up) <= 0.0f) continue;
            const auto target =
                post_contact::target(effective_board, foot[3], body.record.pose[adjacent][3],
                                     state.limbs[limb].part_position, input.wipeout, settings, post);
            if (!target) continue;
            const float distance = dot3(up, sub(*target, foot[3]));
            const std::array<float, 4> offset = {up[0] * distance, up[1] * distance, up[2] * distance,
                                                 up[3] * distance};
            if (physical_solve::solve(body, geometry, adjacent, offset)) {
                foot[3] = *target;
                body.set_part_transform(binding.part, foot);
                Transform adjacent_frame = body.record.pose[adjacent];
                for (std::size_t lane = 0; lane < 4; ++lane) adjacent_frame[3][lane] += offset[lane];
                body.set_part_transform(adjacent, adjacent_frame);
                updated[limb] = true;
            }
        }
    }
}

}  // namespace

std::array<bool, 4> update(FootIkState& state, SkeletonBody& body, const drive::Geometry& geometry,
                           const settings::Settings& settings, const post_contact::SettingsPost& post,
                           const Input& input) {
    std::array<bool, 4> updated{};
    if (input.state_id != 702) {
        const bool airborne = input.state_id == 200 || input.state_id == 201;
        if ((!airborne || input.board_body_flag_868) && input.category_id != 500) {
            feet(state, body, geometry, settings, post, input, updated);
        }
        if (airborne && input.board_body_flag_868) {
            for (std::size_t limb = 2; limb < 4; ++limb) {
                if (state.frames[limb].within_contact_bounds) {
                    state.frames[limb].world = compose_affine(input.board, state.frames[limb].board);
                    const std::size_t part = LIMBS[limb].part;
                    const auto offset = sub(state.frames[limb].world[3], body.record.pose[part][3]);
                    if (physical_solve::solve(body, geometry, part, offset)) {
                        body.set_part_transform(part, state.frames[limb].world);
                        updated[limb] = true;
                    }
                }
            }
        }
    }
    //Always run the external-target pass, including state702.
    for (std::size_t limb = 0; limb < 2; ++limb) {
        if (!(state.limbs[limb].external_blend <= 0.0f)) {
            const auto binding = LIMBS[limb];
            const std::size_t adjacent = binding.parent_part.value();
            const auto offset = sub(state.frames[limb].parent_world[3], body.record.pose[adjacent][3]);
            if (physical_solve::solve(body, geometry, adjacent, offset)) {
                body.set_part_transform(binding.part, state.frames[limb].world);
                body.set_part_transform(adjacent, state.frames[limb].parent_world);
                updated[limb] = true;
            }
        }
    }
    for (std::size_t limb = 0; limb < 4; ++limb) {
        state.external_targets[limb].normal_set = false;
        state.limbs[limb].external_target_set = false;
        state.limbs[limb].local_target_set = false;
    }
    return updated;
}

}  // namespace s3::animation::foot_ik::post_physics
