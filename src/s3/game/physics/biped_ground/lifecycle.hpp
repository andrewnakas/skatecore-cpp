// Port of skate-game/src/physics/biped_ground/lifecycle.rs
//! Ground Post82D32AA0 and complete typed Fill82D32D38/82D785F8.
//! Parent publishes every returned field through the shared output records.
#pragma once
#include <cstdint>
#include <string>

#include "s3/core/player/input_phase.hpp"
#include "s3/core/player/offboard/board_possession/manager.hpp"
#include "s3/core/player/offboard/ground_lifecycle.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::biped_ground::lifecycle {

using FeetState = ::s3::player::offboard::board_possession::manager::State;
namespace core = ::s3::player::offboard::ground_lifecycle;
using ::s3::player::offboard::ground_lifecycle::CollisionInput;
using ::s3::player::offboard::ground_lifecycle::CollisionSettings;
using ::s3::player::offboard::ground_lifecycle::Publication;

/// Apply these AFTER the shared state template/reset, not before it.
/// #[must_use = "Ground State36/40/86 must survive the shared output reset"]
struct [[nodiscard]] StatePublication {
    std::uint32_t word_36 = 0;
    std::uint32_t word_40 = 0;
    bool flag_86 = false;
};

/// Apply every represented record field and return state-template writes for
/// the parent's common publication reset/reconstruction.
/// The actual shared record type is PhysicalPlayerInput, not PhysicsOutputFields.
StatePublication publish_fields(Publication p, ::s3::player::input_phase::PhysicalPlayerInput& physical);

rs::Result<CollisionSettings, std::string> load_collision_settings(const sd::Collections& data);

/// Called AFTER skeleton feedback, before shared FootIK Post consumes requests.
/// `collision` borrows the completed canonical Frame and real Skeleton16336 /
/// SkeletonCollision4072/4056; there is no pre-solve/default observation path.
void post(SkaterRuntime& skater, CollisionInput collision);

/// No output is silently dropped: the full packet includes State36/40/86,
/// OffBoard88/112/116/304/306/307/328/329/330/334, and Reckoning144/164.
/// Borrow the ONE state56 FeetIK owner (its historical type calls feet hands).
rs::Result<Publication, std::string> fill(const SkaterRuntime& skater, const FeetState& feet);

}  // namespace s3::game::physics::biped_ground::lifecycle
