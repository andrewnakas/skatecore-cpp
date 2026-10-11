// Port of skate-game/src/physics/render_pose.rs.
//! Finish the actual physical pose and publish it to the single render skin.
//! Call after collision/error/COM output and before the next animation update.
#include "s3/game/physics/render_pose.hpp"

#include <variant>
#include <vector>

#include "s3/core/animation/foot_ik/post_physics.hpp"
#include "s3/core/animation/output.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"

namespace s3::game::physics::render_pose {

namespace {

using sc::animation::output::PoseBufferError;

/// Rust `{:?}` of `PoseBufferError`.
std::string debug_error(const PoseBufferError& e) {
    return std::visit(
        [](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, PoseBufferError::NonFiniteWeight>) return "NonFiniteWeight";
            if constexpr (std::is_same_v<T, PoseBufferError::InvalidQuaternion>) return "InvalidQuaternion";
            if constexpr (std::is_same_v<T, PoseBufferError::CountOverflow>) return "CountOverflow";
            if constexpr (std::is_same_v<T, PoseBufferError::ShortInput>) return "ShortInput";
            if constexpr (std::is_same_v<T, PoseBufferError::ShortOutput>) return "ShortOutput";
            if constexpr (std::is_same_v<T, PoseBufferError::ShortParents>) return "ShortParents";
            if constexpr (std::is_same_v<T, PoseBufferError::InvalidCacheOffset>) return "InvalidCacheOffset";
            if constexpr (std::is_same_v<T, PoseBufferError::InvalidParent>)
                return "InvalidParent { bone: " + std::to_string(v.bone) + ", parent: " + std::to_string(v.parent) +
                       " }";
            return "";
        },
        e.value);
}

}  // namespace

Status publish(const GamePhysics& physics, SkaterRuntime& skater, std::array<float, 2> average_compression) {
    const auto deck = solve::deck_frame(physics.board);
    const auto& p = skater.player_input.processed;
    // 82DB6DDC/6DF8 passes IsWipeout82DB9100 after state postphysics, not the
    // collision controller's ragdoll mode. Both correction and IK use this result.
    const bool wiping_out = skater.wipeout.requests_wipeout(p);
    // Consume the state producer's sole Skeleton16388 owner. Blocked branches
    // retain it; no per-tick copy may resurrect an already consumed request.
    skater.skeleton_output.correction.apply(skater.skeleton, rs::from_bits(p.vectors_544_560_592_608[0]),
                                            wiping_out, p.flags_2468, p.flags_2476);
    const auto board = skater.skeleton_output.advance_wobble(skater.skeleton, deck);
    skater.foot_ik.post_physics(skater.skeleton, sc::animation::foot_ik::post_physics::Input{
                                                     p.state_2508,
                                                     p.category_2512,
                                                     physics.riding.ground.part_contact_count != 0,
                                                     wiping_out,
                                                     p.flags_2468,
                                                     p.flags_2484,
                                                     p.state_timer_2664,
                                                     p.player_state_value_2520,
                                                     skater.animated_skeleton.roots.world_to_animation,
                                                     board,
                                                 });
    const auto feet = skater.foot_physical.publish(skater.skeleton.record, physics.settings.step.simulation.time_step);
    auto& physical = skater.player_input.physical;
    physical.skeleton.flag_597 = std::uint8_t(skater.foot_ik.state.contacts.support_failed);
    physical.skeleton.flag_600 = std::uint8_t(feet.within_deck_box[0]);
    physical.skeleton.flag_601 = std::uint8_t(feet.within_deck_box[1]);
    const auto& a2w = skater.animated_skeleton.roots.animation_to_world;
    for (std::size_t i = 0; i < 4; ++i) physical.skeleton.anim_to_world_11920[i] = rs::to_bits(a2w[i]);
    physical.reckoning.vector_16 = rs::to_bits(skater.animated_skeleton.board_frames.com_velocity);
    physical.reckoning.vector_64 = rs::to_bits(skater.animated_skeleton.board_frames.centre_of_mass);
    const auto up = physics.riding.reckoning.up;
    physical.reckoning.vector_96 = rs::to_bits(std::array<float, 4>{up.x, up.y, up.z, 0.0f});

    // Use this tick's evaluated animation as the base so unmapped bones keep
    // the real animation. The physical worker changes mapped bones and wheels.
    S3G_TRY(auto globals, skater.animation.evaluator->hierarchy(skater.animation.pose));
    std::vector<sc::animation::output::NativeMatrix> locals;
    locals.reserve(skater.animation.pose.size());
    for (const auto& sqt : skater.animation.pose) locals.push_back(sc::animation::output::sqt_to_matrix(sqt));
    S3G_CHECK(skater.skeleton_output.publish(skater.animated_skeleton, skater.skeleton, physics.board,
                                             physics.settings.step.base_truck_transforms,
                                             skater.ground.steering.targets, average_compression, globals, locals));
    // The renderer consumes render_pose as global matrices. Physical output
    // publishes final locals for mapped bones, so rebuild the complete global
    // hierarchy for every mode, including ragdoll.
    const auto composed = sc::animation::output::compose_hierarchy_in_place(
        std::int32_t(locals.size()), skater.animation.evaluator->frames.parents, 0, locals);
    if (composed.is_err()) return rs::make_err("Physical render hierarchy: " + debug_error(composed.unwrap_err()));
    skater.render_pose = std::move(locals);
    skater.pose_generation = skater.pose_generation + 1;  // wrapping_add on u64
    return ok();
}

}  // namespace s3::game::physics::render_pose
