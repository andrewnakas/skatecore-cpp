// Port of skate-game/src/physics/camera_output.rs.
//! Physical-output publications consumed by gameplay camera82DF69C0.
//! Reset82DE53F0 runs before the actual producers82DB6EC0/82D3A388.
#pragma once
#include <cstdint>
#include <string>

#include "s3/core/animation/physical_feedback.hpp"
#include "s3/core/physics/centre_of_mass_filter.hpp"
#include "s3/core/physics/phase.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/game/camera/publication.hpp"  // stage K: CameraPublicationInputs, Camera*Output, CameraPreferences
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::camera_output {

/// The actual game camera call, after completed physical output conditioning.
/// The user's selected normal High camera is graph type1. This custom world
/// has no road/ledge/camera-volume annotations or other moving actors.
Status advance(const GamePhysics& physics, const SkaterRuntime& skater,
               const sc::animation::physical_feedback::PhysicalFeedback& feedback, camera::CameraRuntime& camera);

/// Call after current state, board/Skeleton and animation conditioning have
/// published, before clearing AnimationControlOutput's per-frame intent bits.
///Selected state owners publish their output before this common consumer.
rs::Result<camera::CameraPublicationInputs, std::string> publish(
    const SkaterRuntime& skater, const sc::physics::phase::PhysicalOutputSnapshot& output,
    const sc::animation::physical_feedback::PhysicalFeedback& feedback,
    sc::physics::centre_of_mass_filter::CentreOfMassOutput com, camera::CameraPreferences preferences,
    std::uint8_t skater_animation_stance, std::uint32_t context, std::uint64_t tick);

/// Private in Rust.
inline std::uint8_t bit(std::uint32_t value, std::uint32_t shift) {
    return static_cast<std::uint8_t>((value >> shift) & 1u);
}

///82DF6A68..6B8C copies the distinct OffBoard trajectory when byte331 is set.
///Keep the owner's actual availability byte: neither state category nor the
///existence of this packet establishes that a trajectory has been produced.
/// Private in Rust (exercised by the ported tests).
camera::CameraOffboardOutput offboard_output(const sc::player::input_phase::OffBoardOutputFields& output);

}  // namespace s3::game::physics::camera_output
