// Port of skate-game/src/physics/offboard/air_selector.rs
//! Concrete BipedAir selector host over the canonical BoardWorld.
//! Parent owns the Air facade and phase schedule. Native completion is explicit,
//! so a synchronous fixed-step backend never adds an automatic frame delay.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "s3/core/air/trajectory.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/player/offboard/air_launch.hpp"
#include "s3/core/player/offboard/air_selector.hpp"
#include "s3/core/player/offboard/ground_query.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/offboard/air_selector/settings.hpp"
#include "s3/game/physics/offboard/contact_toolkit.hpp"
#include "s3/game/physics/offboard/ground_query.hpp"

namespace s3::game::physics::offboard::air_selector {

namespace core = ::s3::player::offboard::air_selector;
using settings::Settings;
using sc::air::trajectory::Prediction;
using sc::air::trajectory::QueryResult;
using sc::physics::board_world::BoardWorld;
using sc::player::offboard::air_launch::Packet;
using core::Context;
using core::Frame;
using core::Vector;

struct AirSelector {
    core::Selector core = core::Selector::default_();
    Settings settings{};
    // Private in Rust (kept public for the ported in-module tests).
    std::optional<std::vector<QueryResult>> completed_launch;
    std::optional<Prediction> completed_requery;

    void reset();
    static AirSelector new_(Settings settings);
    ///82D6CA58 submits; results stay staged until consume_launch at native Sync.
    rs::Result<rs::Unit, const char*> launch(const BoardWorld& world, Packet packet, Vector gravity, Context context);
    ///82D6CC50/D020/D4D0. The current authored level has no dynamic edge
    ///providers; static metadata remains mandatory and is never fabricated.
    rs::Result<std::optional<std::size_t>, const char*> consume_launch(const BoardWorld& world, Context context,
                                                                       float deck_half_wheelbase);
    void adjust_animation(std::int32_t frame, Vector animation, Frame axes);
    void sample(std::int32_t frame, float dt, core::TrajectoryResult& out);
    ///82D6E798 permits both pending8492 and8499. Its destination is the same
    ///query slot0, so the later requery replaces that slot before consumption.
    rs::Result<bool, const char*> requery(const BoardWorld& world, Context context, std::uint32_t flags_2472,
                                          std::uint32_t flags_2488);
    rs::Result<bool, const char*> consume_requery();
    ///82DB3C78: clear ready before launch/requery consumption, then rearm.
    ///Call at the original selector consume phase; never use elapsed frames as
    ///a synthetic readiness condition for the synchronous world backend.
    rs::Result<std::optional<std::size_t>, const char*> consume(const BoardWorld& world, Context context,
                                                                float deck_half_wheelbase);
    void exit();
};

}  // namespace s3::game::physics::offboard::air_selector
