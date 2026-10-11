// Port of skate-game/src/physics/skater.rs.
#include "s3/game/physics/skater.hpp"

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/animation_phase.hpp"
#include "s3/game/physics/landing_quality.hpp"
#include "s3/game/physics/skeleton_body.hpp"

namespace s3::game::physics::skater {

using sc::physics::board::BodyId;
using sc::physics::skeleton_animation_record::IDENTITY;
using sc::physics::skeleton_animation_record::map_animation_parts;
using skater_animation::SkaterAnimation;

Status SkaterRuntime::travel_to(std::array<std::array<float, 4>, 4> transform) {
    S3G_CHECK(player_input.request_teleport(transform));
    teleport_state.request_manual(transform, true);
    return ok();
}

rs::Result<SkaterRuntime, std::string> SkaterRuntime::load(const sd::Vfs& vfs, const std::string& asset_root,
                                                           const graph_runtime::StockGraphs& graphs,
                                                           const GamePhysics& physics, const std::string& mode) {
    return load_for_world(vfs, asset_root, graphs, physics, mode, nullptr);
}

rs::Result<SkaterRuntime, std::string> SkaterRuntime::load_for_world(
    const sd::Vfs& vfs, const std::string& asset_root, const graph_runtime::StockGraphs& graphs,
    const GamePhysics& physics, const std::string& mode,
    std::shared_ptr<const skater_animation::AnimationSource> source) {
    S3G_TRY(auto data, sd::Collections::load(vfs, asset_root));
    S3G_TRY(auto banks, sd::AnimationBanks::load(vfs, asset_root));
    S3G_TRY(auto animation_metadata, banks.metadata());
    // The host's current character is a custom skater with no pro selector
    // or equipped physical hat. These are profile choices, not force values.
    std::optional<SkaterAnimation> animation_opt;
    if (source) {
        S3G_TRY(auto a, SkaterAnimation::from_source(data, graphs, "", source));
        animation_opt.emplace(std::move(a));
    } else {
        S3G_TRY(auto a, SkaterAnimation::load(vfs, asset_root, data, graphs, ""));
        animation_opt.emplace(std::move(a));
    }
    SkaterAnimation animation = std::move(*animation_opt);
    S3G_TRY(auto initial_hierarchy, animation.evaluate_initial_pose());
    S3G_TRY(auto animated_skeleton,
            animated_skeleton::AnimatedSkeleton::load(vfs, asset_root, data, animation.evaluator->frames, false));
    std::array<sc::physics::skeleton_animation_record::AnimationPartTransform, 24> initial_parts{};
    {
        auto mapped = map_animation_parts(initial_hierarchy, animated_skeleton.bone_indices,
                                          animated_skeleton.physics_frames);
        if (mapped.is_err()) return rs::make_err(std::string(mapped.unwrap_err()));
        initial_parts = mapped.unwrap();
    }
    const auto deck = physics.board.part_transforms()[sc::physics::board::index(BodyId::Deck)];
    auto spawn = IDENTITY;
    for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t k = 0; k < 3; ++k) spawn[column][k] = deck.basis.columns[column][k];
    }
    spawn[3] = {deck.translation.x, deck.translation.y, deck.translation.z, 0.0f};
    const auto& frames = animation.evaluator->frames;
    S3G_TRY(auto skeleton, skeleton_body::load(vfs, asset_root, data, frames.source_sha256, initial_parts, spawn,
                                               physics.settings.step.simulation, std::nullopt));
    animated_skeleton.roots.reset_initial_alignment(skeleton.animation_to_world);
    animated_skeleton.board_frames.reset(spawn);
    for (int i = 0; i < 2; ++i) {
        animated_skeleton.board_frames.publish_centre_of_mass(skeleton.record.centre_of_mass,
                                                              physics.settings.step.simulation.time_step, 0);
    }
    animated_skeleton.board_frames.publish_local_observations(animated_skeleton.roots, spawn);
    S3G_TRY(auto animation_input, animation_input::AnimationInput::load(data, frames, mode));
    S3G_TRY(auto skeleton_joints, skeleton_body::load_joints(vfs, asset_root, data, frames.source_sha256,
                                                             initial_hierarchy, frames.parents,
                                                             animated_skeleton.bone_indices));
    S3G_TRY(auto skeleton_drives,
            skeleton_body::load_drives(vfs, asset_root, data, frames.source_sha256, initial_hierarchy,
                                       animated_skeleton.bone_indices, initial_parts, skeleton_joints,
                                       skeleton.animation_to_world, spawn, physics.settings.step.simulation));
    // Reset82BD9990 finishes by publishing both COM targets to the
    // actual extra bodies. These bodies subsequently share the solver.
    const auto initial_targets = skeleton_drives.targets.update_extra_targets(
        skeleton, animated_skeleton.board_frames.com_frame, animated_skeleton.board_frames.lifted_com_frame);
    S3G_TRY(auto foot_ik, foot_ik::FootIk::load(data, frames, animated_skeleton));
    S3G_TRY(auto skeleton_output, skeleton_output::SkeletonOutput::load(data, frames, animated_skeleton));
    // Actor82591448 passes network || ghost for self-pair suppression.
    // This local game has neither networking nor a ghost actor.
    S3G_TRY(auto skeleton_collision,
            skeleton_body::load_collision(vfs, asset_root, data, frames.source_sha256, false));
    S3G_TRY(auto wipeout_state, wipeout_states::WipeoutState::load(data, vfs, asset_root, frames.source_sha256));
    S3G_TRY(auto respawn, respawn::Runtime::load(data, spawn, animation.checkpoint_stance()));
    S3G_TRY(auto trajectory, air_trajectory::AirTrajectoryRuntime::load(data));
    trajectory.bind_grind_world(physics.grind_world);
    S3G_TRY(auto skeleton_input, skeleton_input_runtime::SkeletonInputRuntime::load(data));
    skeleton_input.drive_frames = initial_parts;
    skeleton_input.extra_target_positions = {initial_targets.com, initial_targets.lifted_com,
                                             initial_targets.following_com};

    // Struct-literal fields in Rust declaration order of the initializer (left to right).
    S3G_TRY(auto scoring, scoring_runtime::Runtime::load(data));
    S3G_TRY(auto climbing, climbing::Runtime::load(vfs, asset_root, frames.bone_names));
    S3G_TRY(auto landing_quality_settings, landing_quality::load(data));
    S3G_TRY(auto skeleton_air, skeleton_air::SkeletonAir::load(data));
    S3G_TRY(auto air_reckoning, air_reckoning::AirReckoning::load(data));
    S3G_TRY(auto air_settings, air_phase::AirSettings::load(data));
    S3G_TRY(auto known_air, known_air::KnownAir::load(data));
    S3G_TRY(auto biped_air, biped_air::BipedAir::load(data));
    S3G_TRY(auto landing_on_deck, landing_on_deck::Runtime::load(data));
    S3G_TRY(auto landing_deck, offboard::landing_deck::Owner::load(data));
    S3G_TRY(auto ground_animation_settings, ground_animation::GroundAnimationSettings::load(data));
    S3G_TRY(auto revert_state, revert_state::RevertState::load(data));
    S3G_TRY(auto slide_state, slide_state::SlideState::load(data));
    S3G_TRY(auto grind, grind::Runtime::load(data));
    S3G_TRY(auto footplant, footplant::Footplant::load(data));
    S3G_TRY(auto boneless, boneless::Boneless::load(data));
    S3G_TRY(auto handplant, handplant::Handplant::load(data));
    S3G_TRY(auto wipeout, wipeout::Wipeout::load(data));
    teleport_state::Checkpoint checkpoint{};
    checkpoint.transform = spawn;
    checkpoint.on_board = true;
    auto teleport = teleport_state::Runtime::new_(checkpoint);
    S3G_TRY(auto collision_feedback, skeleton_body::load_feedback(data, skeleton_collision.settings));
    S3G_TRY(auto foot_physical, foot_physical_output::FootPhysicalOutputs::load(data));
    S3G_TRY(auto player_input, player_input::PlayerInputRuntime::load(data));
    S3G_TRY(auto player_state, player_state::PlayerState::load(data, mode));
    S3G_TRY(auto ground, ground_runtime::GroundState::load(data, mode, true));
    S3G_TRY(auto ground_runtime, ground_runtime::GroundRuntime::load(data));
    S3G_TRY(auto ground_profiles, ground_runtime::GroundProfiles::load(data));
    S3G_TRY(auto ground_settings, ground_runtime::GroundSettings::load(data, mode, "smooth"));
    S3G_TRY(auto biped_ground, biped_ground::Owner::load(data, animation_metadata));
    S3G_TRY(auto air_selector_settings, offboard::air_selector::Settings::load(data));
    S3G_TRY(auto animation_feedback, animation_feedback::AnimationFeedback::load(data));
    S3G_TRY(auto board_possession, offboard::board_manager::Owner::load(data));
    S3G_TRY(auto board_possession_live, offboard::board_manager::runtime::LiveState::load(data, physics));

    sc::physics::skeleton_body::SkeletonPoseErrors pose_errors{};
    pose_errors.targets = {initial_targets.com, initial_targets.lifted_com, initial_targets.following_com};

    return SkaterRuntime{
        std::move(scoring),
        std::move(climbing),
        std::move(initial_hierarchy),
        0,
        sc::physics::centre_of_mass_filter::CentreOfMassFilter{},
        // PhysOut reset82DE53F0 clears these observations before first output.
        sc::physics::centre_of_mass_filter::CentreOfMassOutput{},
        std::move(animation),
        std::move(animated_skeleton),
        std::move(skeleton_air),
        std::move(air_reckoning),
        sc::air::state::PhysicsAirState{},
        std::move(air_settings),
        std::move(known_air),
        std::move(biped_air),
        std::move(landing_on_deck),
        std::move(landing_deck),
        ground_animation::GroundAnimationRuntime{},
        std::move(ground_animation_settings),
        std::move(revert_state),
        std::move(slide_state),
        std::move(trajectory),
        grind_camera::GrindCamera{},
        std::move(grind),
        std::move(footplant),
        std::move(boneless),
        std::move(handplant),
        std::move(wipeout),
        std::move(wipeout_state),
        std::move(respawn),
        std::move(teleport),
        std::move(skeleton),
        std::move(skeleton_joints),
        std::move(skeleton_drives),
        std::move(skeleton_collision),
        std::move(collision_feedback),
        pose_errors,
        {0.0f, 0.0f, 0.0f, 0.0f},
        {},
        std::nullopt,
        std::nullopt,
        std::move(foot_ik),
        std::move(foot_physical),
        std::move(player_input),
        std::move(player_state),
        std::move(skeleton_input),
        std::move(ground),
        std::move(ground_runtime),
        std::move(ground_profiles),
        std::make_shared<const ground_runtime::GroundSettings>(std::move(ground_settings)),
        ground_phase::GroundLifecycle::new_(),
        std::move(biped_ground),
        offboard::contact_toolkit::Owner{},
        offboard::air_selector::AirSelector::new_(std::move(air_selector_settings)),
        sc::player::offboard::board_possession::manager::State{},
        biped_ground::grab_runtime::Owner{},
        std::move(animation_input),
        std::move(animation_feedback),
        animation_phase::initial_feedback(),
        sc::animation::landing_quality::Output{},
        std::move(landing_quality_settings),
        std::move(skeleton_output),
        skateboard_controller::SkateboardController::new_(),
        std::move(board_possession),
        std::move(board_possession_live),
    };
}

}  // namespace s3::game::physics::skater
