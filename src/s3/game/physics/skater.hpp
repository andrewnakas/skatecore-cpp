// Port of skate-game/src/physics/skater.rs.
//! The game's single skater: stock graph animation and its physical skeleton.
//! Loading creates the real bodies from the animator's initial evaluated pose.
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "s3/core/air/state.hpp"
#include "s3/core/animation/landing_quality.hpp"
#include "s3/core/animation/output/native.hpp"
#include "s3/core/animation/physical_feedback.hpp"
#include "s3/core/physics/centre_of_mass_filter.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/offboard/board_possession/manager.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/common.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/scoring_runtime.hpp"
#include "s3/game/skater_animation.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/air_phase.hpp"
#include "s3/game/physics/air_reckoning.hpp"
#include "s3/game/physics/air_trajectory.hpp"
#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/animation_feedback.hpp"
#include "s3/game/physics/animation_input.hpp"
#include "s3/game/physics/biped_air.hpp"
#include "s3/game/physics/biped_ground.hpp"
#include "s3/game/physics/biped_ground/grab_runtime.hpp"
#include "s3/game/physics/boneless.hpp"
#include "s3/game/physics/climbing.hpp"
#include "s3/game/physics/foot_ik.hpp"
#include "s3/game/physics/foot_physical_output.hpp"
#include "s3/game/physics/footplant.hpp"
#include "s3/game/physics/grind.hpp"
#include "s3/game/physics/grind_camera.hpp"
#include "s3/game/physics/ground_animation.hpp"
#include "s3/game/physics/ground_phase.hpp"
#include "s3/game/physics/ground_runtime.hpp"
#include "s3/game/physics/handplant.hpp"
#include "s3/game/physics/known_air.hpp"
#include "s3/game/physics/landing_on_deck.hpp"
#include "s3/game/physics/offboard/air_selector.hpp"
#include "s3/game/physics/offboard/board_manager.hpp"
#include "s3/game/physics/offboard/board_manager/runtime.hpp"
#include "s3/game/physics/offboard/contact_toolkit.hpp"
#include "s3/game/physics/offboard/landing_deck.hpp"
#include "s3/game/physics/player_input.hpp"
#include "s3/game/physics/player_state.hpp"
#include "s3/game/physics/respawn.hpp"
#include "s3/game/physics/revert_state.hpp"
#include "s3/game/physics/skateboard_controller.hpp"
#include "s3/game/physics/skeleton_air.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"
#include "s3/game/physics/skeleton_output.hpp"
#include "s3/game/physics/slide_state.hpp"
#include "s3/game/physics/teleport_state.hpp"
#include "s3/game/physics/wipeout.hpp"
#include "s3/game/physics/wipeout_states.hpp"

namespace s3::game::physics::skater {

struct SkaterRuntime {
    scoring_runtime::Runtime scoring;
    climbing::Runtime climbing;
    /// Completed physical pose in native animation space, read by rendering.
    std::vector<sc::animation::output::NativeMatrix> render_pose;
    std::uint64_t pose_generation = 0;
    sc::physics::centre_of_mass_filter::CentreOfMassFilter centre_of_mass_filter{};
    sc::physics::centre_of_mass_filter::CentreOfMassOutput centre_of_mass_output{};
    skater_animation::SkaterAnimation animation;
    animated_skeleton::AnimatedSkeleton animated_skeleton;
    skeleton_air::SkeletonAir skeleton_air;
    air_reckoning::AirReckoning air_reckoning;
    sc::air::state::PhysicsAirState air_state{};
    air_phase::AirSettings air_settings;
    known_air::KnownAir known_air;
    biped_air::BipedAir biped_air;
    landing_on_deck::Runtime landing_on_deck;
    offboard::landing_deck::Owner landing_deck;
    ground_animation::GroundAnimationRuntime ground_animation{};
    ground_animation::GroundAnimationSettings ground_animation_settings;
    revert_state::RevertState revert_state;
    slide_state::SlideState slide_state;
    air_trajectory::AirTrajectoryRuntime trajectory;
    grind_camera::GrindCamera grind_camera{};
    grind::Runtime grind;
    footplant::Footplant footplant;
    boneless::Boneless boneless;
    handplant::Handplant handplant;
    wipeout::Wipeout wipeout;
    wipeout_states::WipeoutState wipeout_state;
    respawn::Runtime respawn;
    teleport_state::Runtime teleport_state;
    sc::physics::skeleton_body::SkeletonBody skeleton;
    sc::physics::skeleton_body::SkeletonJoints skeleton_joints;
    sc::physics::skeleton_body::SkeletonDrives skeleton_drives;
    sc::physics::skeleton_body::SkeletonCollisionMode skeleton_collision;
    sc::physics::skeleton_body::SkeletonCollisionFeedback collision_feedback;
    sc::physics::skeleton_body::SkeletonPoseErrors pose_errors{};
    std::array<float, 4> collision_pose_error{};
    /// Skeleton16288/16304, retained by the post-solver response producer.
    std::array<std::array<float, 4>, 2> collision_extra_errors{};
    ///Skeleton16384 is published by the completed pose-error response. Wipeout
    ///runs after that publication; no fabricated pre-solve measurement exists.
    std::optional<float> collision_maximum_error;
    std::optional<sc::physics::skeleton_body::SkeletonDriveBatch> solved_drives;
    foot_ik::FootIk foot_ik;
    foot_physical_output::FootPhysicalOutputs foot_physical;
    player_input::PlayerInputRuntime player_input;
    player_state::PlayerState player_state;
    skeleton_input_runtime::SkeletonInputRuntime skeleton_input;
    ground_runtime::GroundState ground;
    ground_runtime::GroundRuntime ground_runtime;
    ground_runtime::GroundProfiles ground_profiles;
    std::shared_ptr<const ground_runtime::GroundSettings> ground_settings;
    ground_phase::GroundLifecycle ground_lifecycle;
    biped_ground::Owner biped_ground;
    offboard::contact_toolkit::Owner offboard_contact{};
    offboard::air_selector::AirSelector offboard_air_selector;
    sc::player::offboard::board_possession::manager::State offboard_feet{};
    biped_ground::grab_runtime::Owner offboard_grab{};
    animation_input::AnimationInput animation_input;
    animation_feedback::AnimationFeedback animation_feedback;
    sc::animation::physical_feedback::PhysicalFeedback physical_feedback{};
    sc::animation::landing_quality::Output landing_quality{};
    sc::animation::landing_quality::Settings landing_quality_settings{};
    skeleton_output::SkeletonOutput skeleton_output;
    skateboard_controller::SkateboardController skateboard_controller;
    offboard::board_manager::Owner board_possession;
    offboard::board_manager::runtime::LiveState board_possession_live;

    Status travel_to(std::array<std::array<float, 4>, 4> transform);
    static rs::Result<SkaterRuntime, std::string> load(const sd::Vfs& vfs, const std::string& asset_root,
                                                       const graph_runtime::StockGraphs& graphs,
                                                       const GamePhysics& physics, const std::string& mode);
    static rs::Result<SkaterRuntime, std::string> load_for_world(
        const sd::Vfs& vfs, const std::string& asset_root, const graph_runtime::StockGraphs& graphs,
        const GamePhysics& physics, const std::string& mode,
        std::shared_ptr<const skater_animation::AnimationSource> source);

    // physics.rs `impl SkaterRuntime`.
    /// Observe the current grind publication without adding a second grind owner.
    std::tuple<bool, std::string_view, std::uint32_t, float> mod_grind() const;
};

}  // namespace s3::game::physics::skater
