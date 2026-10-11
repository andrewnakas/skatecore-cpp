// Port of skate-game/src/physics/ground_runtime/state.rs.
//! Native Ground controller ownership. Source entry/update and output retain
//! the same instances; constructing this does not mark Ground as entered.
//! GroundState also carries `enter` (entry.rs) and `update` (update.rs).
#pragma once
#include <array>
#include <string>
#include <string_view>

#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/manual/state.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/core/riding/grounded/state.hpp"
#include "s3/core/riding/grounded/state/board.hpp"
#include "s3/core/riding/grounded/state/output.hpp"
#include "s3/core/riding/pumping/state.hpp"
#include "s3/core/riding/speed_model.hpp"
#include "s3/core/riding/speed_wobble.hpp"
#include "s3/core/riding/steering.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/ground_runtime/entry.hpp"
#include "s3/game/physics/ground_runtime/pumping.hpp"
#include "s3/game/physics/ground_runtime/services.hpp"
#include "s3/game/physics/ground_runtime/settings.hpp"
#include "s3/game/physics/ground_runtime/update.hpp"

namespace s3::game::physics::animation_input {
struct AnimationInput;  // physics/animation_input.hpp (group D)
}
namespace s3::game::physics::ground_runtime {
struct GroundRuntime;  // physics/ground_runtime.hpp
}

namespace s3::game::physics::ground_runtime::state {

using sc::riding::grounded::state::output::GroundOutputSettings;
using sc::riding::grounded::state::output::PhysicsGroundOutput;

struct GroundState {
    sc::riding::grounded::state::PhysicsGroundState state;
    sc::riding::pumping::state::PumpingState pumping;
    sc::riding::speed_wobble::SpeedWobbleState wobble;
    sc::physics::manual::state::ManualState manual;
    sc::riding::steering::TruckSteeringState steering;
    sc::riding::speed_model::SpeedModelState speed;
    float heading_previous = 0.0f;
    pumping::GroundPumping pumping_settings;
    GroundOutputSettings output_settings;
    std::array<bool, 5> auto_push_enabled{};
    bool entered = false;
    entry::EntrySettings entry_settings;

    static rs::Result<GroundState, std::string> load(const sd::Collections& data, std::string_view mode,
                                                     bool human_player);
    PhysicsGroundOutput output(const sc::player::input_phase::ProcessedPhysicsInput& p,
                               const animation_input::AnimationInput& animation,
                               const sc::physics::board_toolkit::BoardToolkit& t) const;
    services::GroundControllers controllers();
    /// ground_runtime/entry.rs.
    Status enter(sc::physics::board_runtime::BoardRuntime& board,
                 const sc::player::input_phase::ProcessedPhysicsInput& processed,
                 const animation_input::AnimationInput& animation,
                 const sc::physics::board_toolkit::BoardToolkit& toolkit, entry::GroundEntryTargets targets);
    /// ground_runtime/update.rs.
    rs::Result<sc::riding::grounded::state::board::GroundBoardOutcome, std::string> update(
        GroundRuntime& runtime, sc::physics::board_runtime::BoardRuntime& board,
        const sc::physics::board_world::BoardWorld& world, const settings::GroundSettings& settings,
        update::GroundUpdateFrame frame, services::GroundPhysicalFrame physical,
        update::GroundUpdateTargets targets);
};

}  // namespace s3::game::physics::ground_runtime::state
