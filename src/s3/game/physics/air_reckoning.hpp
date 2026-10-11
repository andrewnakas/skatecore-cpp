// Port of skate-game/src/physics/air_reckoning.rs.
//! Production stock settings and live-input adapter for Reckoning82D8DBD8.
#pragma once
#include <array>
#include <string>

#include "s3/core/air/reckoning.hpp"
#include "s3/core/air/state.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/air_reckoning/settings.hpp"

namespace s3::game::physics::riding_outputs {
struct RidingOutputs;  // physics/riding_outputs.hpp (group D)
}

namespace s3::game::physics::air_reckoning {

using riding_outputs::RidingOutputs;
using ::s3::air::reckoning::AirState;
using ::s3::air::state::PhysicsAirReckoningFields;
using ::s3::player::input_phase::ProcessedPhysicsInput;

struct AirReckoning {
    AirState state;
    // Private in Rust.
    ::s3::air::reckoning::Settings settings;
    std::array<settings::Mode, 5> modes{};

    void update_plant(RidingOutputs& riding, const ProcessedPhysicsInput& p, std::array<float, 4> up,
                      std::array<float, 4> heading);
    static rs::Result<AirReckoning, std::string> load(const sd::Collections& data);
    ///The Ground/PhysicsAir lifecycles borrow this canonical pair for their
    ///native spin resets. Filters and matrix1008 remain owned by RidingOutputs.
    PhysicsAirReckoningFields fields(const RidingOutputs& riding) const;
    ///Called by the actual PhysicsAir/KnownAir update before its Board phase.
    ///Return the newly published normal1216, not the caller's earlier snapshot.
    rs::Result<PhysicsAirReckoningFields, std::string> update(RidingOutputs& riding,
                                                              const ProcessedPhysicsInput& processed,
                                                              float physical_body_spin,
                                                              std::array<float, 4> landing_normal, float normal_blend,
                                                              float target_spin, float flip_request);
};

}  // namespace s3::game::physics::air_reckoning
