// Port of skate-game/src/physics/air_trajectory/mod.rs.
//! Real stock-settings and BoardWorld adapter for the player trajectory selector.
#include "s3/game/physics/air_trajectory.hpp"

namespace s3::game::physics::air_trajectory {

using ::s3::air::trajectory::query_trajectory;
using Vector = std::array<float, 4>;

rs::Result<QueryResult, std::string> AirTrajectoryRuntime::query(const BoardWorld& world, QueryRequest request) {
    return query_trajectory<std::string>(
        request, [&](Vector start, Vector end, float radius) { return world::line(world, start, end, radius); },
        [&](Vector position, float radius) { return world::nearby(world, position, radius); });
}

rs::Result<AirTrajectoryRuntime, std::string> AirTrajectoryRuntime::load(const sd::Collections& collections) {
    AirTrajectoryRuntime out;
    out.selector = TrajectorySelector::new_();
    S3G_TRY(out.settings, settings::load(collections));
    out.pending_results = std::nullopt;
    S3G_TRY(out.grind_settings, grind::Settings::load(collections));
    out.grind_world = nullptr;
    out.nearby_grinds.clear();
    return out;
}

rs::Result<bool, std::string> AirTrajectoryRuntime::launch(LaunchInfo info, SelectorInput input,
                                                           const BoardWorld& world) {
    S3G_TRY(const bool launched, selector.launch(info, input, settings));
    if (launched) {
        S3G_CHECK(submit(world));
    }
    return launched;
}

void AirTrajectoryRuntime::bind_grind_world(std::shared_ptr<const StaticProvider> provider) {
    grind_world = std::move(provider);
    nearby_grinds.clear();
}

rs::Result<bool, std::string> AirTrajectoryRuntime::update(SelectorInput input, const BoardWorld& world,
                                                           GrindContext context) {
    if (!pending_results) {
        return selector.update_without_completion();
    }
    const std::vector<QueryResult> results = std::move(*pending_results);
    pending_results = std::nullopt;
    if (!grind_world) {
        return rs::make_err(std::string("Trajectory static grind provider was not registered"));
    }
    const StaticProvider& provider = *grind_world;
    S3G_TRY(const bool valid,
            selector.complete_batch(
                results, input, settings,
                [&](::s3::air::trajectory::Prediction& prediction, bool acquire) {
                    return grind_settings.evaluate(prediction, acquire, world, provider, nearby_grinds,
                                                   input.grind_lock_distance, context);
                },
                [&](Vector start, Vector end, float radius) { return world::line(world, start, end, radius); }));
    if (selector.pending()) {
        S3G_CHECK(submit(world));
    }
    return valid;
}

Status AirTrajectoryRuntime::submit(const BoardWorld& world) {
    std::vector<QueryResult> results;
    for (const auto& request : selector.requests()) {
        S3G_TRY(auto r, query(world, request));
        results.push_back(std::move(r));
    }
    pending_results = std::move(results);
    return ok();
}

}  // namespace s3::game::physics::air_trajectory
