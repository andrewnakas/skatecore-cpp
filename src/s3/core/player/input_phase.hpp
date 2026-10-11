// Port of skate-core/src/player/input_phase/mod.rs.
// Skate 3 TU3 `PhysicalPlayerHiLOD::Input` (`0x82DB4048`).
//
// The phase is split into direct field publication and an ordered runtime.
// Separate native subsystems remain mandatory service calls; none have a
// fallback implementation here.
//
// Submodules live in input_phase/<sub>.hpp (namespace input_phase::<sub>); this header
// re-exports them exactly like mod.rs's `pub use` list.
#pragma once
#include "s3/core/player/input_phase/air_output.hpp"
#include "s3/core/player/input_phase/grind_input.hpp"
#include "s3/core/player/input_phase/grind_output.hpp"
#include "s3/core/player/input_phase/motion_math.hpp"
#include "s3/core/player/input_phase/pose_output.hpp"
#include "s3/core/player/input_phase/publication.hpp"
#include "s3/core/player/input_phase/requests.hpp"
#include "s3/core/player/input_phase/runtime.hpp"
#include "s3/core/player/input_phase/types.hpp"

namespace s3::player::input_phase {

// pub use air_output::AirOutputFields;
using air_output::AirOutputFields;
// pub use grind_input::GrindInvestigationFields;
using grind_input::GrindInvestigationFields;
// pub use grind_output::GrindOutputFields;
using grind_output::GrindOutputFields;
// pub use pose_output::{AnimationOutputFields, ScoringOutputFields, SkeletonOutputFields};
using pose_output::AnimationOutputFields;
using pose_output::ScoringOutputFields;
using pose_output::SkeletonOutputFields;
// pub use requests::*;
using requests::GroundHistoryRequest;
using requests::GroundHistoryResult;
using requests::PrepareJumpRequest;
// pub use runtime::{InputContinuation, InputPhaseError, InputPhaseServices, finish_input,
//                   process_input, start_input};
using runtime::finish_input;
using runtime::InputContinuation;
using runtime::InputPhaseError;
using runtime::InputPhaseServices;
using runtime::process_input;
using runtime::start_input;
// pub use types::*;
using types::AnimationInputPacket;
using types::CollisionOutputFields;
using types::CurrentStateFields;
using types::empty_external_physics;
using types::GroundOutputFields;
using types::LineTestFields;
using types::NativeReferenceBase;
using types::NativeRelativeReference;
using types::OffBoardOutputFields;
using types::PhysicalPlayerInput;
using types::PhysicsOutputFields;
using types::PlayerInputState;
using types::ProbeFields;
using types::ProcessedPhysicsInput;
using types::ProcessedPhysicsSnapshot;
using types::RawMatrix;
using types::RawVector;
using types::SkateboardMotionFields;
using types::StateVariantFields;
using types::SystemReckoningFields;

}  // namespace s3::player::input_phase
