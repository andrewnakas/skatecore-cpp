// Port of skate-game/src/physics/ground_runtime/pumping.rs.
//! Actual Ground82D37D34..7EAC pumping preparation and stock bindings.
#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/core/riding/pumping/settings.hpp"
#include "s3/core/riding/pumping/state.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::riding_outputs {
struct RidingOutputs;  // physics/riding_outputs.hpp (group D)
}
namespace s3::game::physics::animated_skeleton {
struct AnimatedSkeleton;  // physics/animated_skeleton.hpp (group D)
}

namespace s3::game::physics::ground_runtime::pumping {

using animated_skeleton::AnimatedSkeleton;
using riding_outputs::RidingOutputs;
using sc::physics::board_toolkit::BoardToolkit;
using sc::riding::pumping::settings::PumpingMode;
using sc::riding::pumping::settings::PumpingSettings;
using sc::riding::pumping::state::PumpingState;

/// `#[derive(Clone, Copy)]`
struct GroundPumpingMode {
    PumpingMode controller{};
    /// Selected physics_mode layout+8, consumed by CalcPumpForce82D933F0.
    float unintentional_scalar = 0.0f;
};

struct GroundPumping {
    PumpingSettings settings{};
    std::array<GroundPumpingMode, 5> modes{};

    ///Revert82D435FC..36A0 uses the processed timestep and flag2472 bit16.
    Status update_revert(PumpingState& state, const BoardToolkit& toolkit, const RidingOutputs& riding,
                         const AnimatedSkeleton& skeleton, const sc::player::input_phase::ProcessedPhysicsInput& p,
                         float balance) const;
    static rs::Result<GroundPumping, std::string> load(const sd::Collections& data);
    /// Ground82D37E74 reads the current Processed2548 mode, not the startup mode.
    rs::Result<GroundPumpingMode, std::string> mode(std::uint32_t index) const;
    ///Runs after contact-state update and before Ground::UpdateSkateboard.
    void update(PumpingState& state, const BoardToolkit& toolkit, const RidingOutputs& riding,
                const AnimatedSkeleton& skeleton, std::uint32_t flags_2476, GroundPumpingMode mode) const;
};

/// Private helpers.
float wrap(float angle);
sc::math::Vector3 xyz(std::array<float, 4> v);
std::array<float, 4> v(sc::math::Vector3 v);

}  // namespace s3::game::physics::ground_runtime::pumping
