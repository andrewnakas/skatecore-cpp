// Port of skate-game/src/physics/air_trajectory/mod.rs.
//! Real stock-settings and BoardWorld adapter for the player trajectory selector.
#pragma once
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/air/trajectory.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/grind_world.hpp"
#include "s3/game/physics/air_trajectory/grind.hpp"
#include "s3/game/physics/air_trajectory/settings.hpp"
#include "s3/game/physics/air_trajectory/world.hpp"

namespace s3::game::physics::air_trajectory {

using grind::GrindContext;
using ::s3::air::trajectory::LaunchInfo;
using ::s3::air::trajectory::QueryRequest;
using ::s3::air::trajectory::QueryResult;
using ::s3::air::trajectory::SelectorInput;
using ::s3::air::trajectory::SelectorSettings;
using ::s3::air::trajectory::TrajectorySelector;
using ::s3::game::grind_world::StaticProvider;
using ::s3::physics::board_world::BoardWorld;

struct AirTrajectoryRuntime {
    TrajectorySelector selector;
    SelectorSettings settings;
    // Private in Rust.
    std::optional<std::vector<QueryResult>> pending_results;
    grind::Settings grind_settings;
    std::shared_ptr<const StaticProvider> grind_world;  // Option<Arc<StaticProvider>>: nullptr = None
    std::vector<std::size_t> nearby_grinds;

    ///Full82E099A0 query shared by trajectory and Footplant callers.
    static rs::Result<QueryResult, std::string> query(const BoardWorld& world, QueryRequest request);
    static rs::Result<AirTrajectoryRuntime, std::string> load(const sd::Collections& collections);
    rs::Result<bool, std::string> launch(LaunchInfo info, SelectorInput input, const BoardWorld& world);
    void bind_grind_world(std::shared_ptr<const StaticProvider> provider);
    rs::Result<bool, std::string> update(SelectorInput input, const BoardWorld& world, GrindContext context);
    // Private in Rust.
    Status submit(const BoardWorld& world);
};

}  // namespace s3::game::physics::air_trajectory
