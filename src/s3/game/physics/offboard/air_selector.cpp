// Port of skate-game/src/physics/offboard/air_selector.rs
#include "s3/game/physics/offboard/air_selector.hpp"

#include <sstream>

namespace s3::game::physics::offboard::air_selector {

namespace ledge = ::s3::player::offboard::air_selector::ledge;
using sc::player::offboard::ground_query::Edge;
using sc::player::offboard::ground_query::Line;

void AirSelector::reset() {
    core = core::Selector::default_();
    completed_launch = std::nullopt;
    completed_requery = std::nullopt;
}

AirSelector AirSelector::new_(Settings settings) {
    AirSelector s;
    s.core = core::Selector::default_();
    s.settings = settings;
    return s;
}

rs::Result<rs::Unit, const char*> AirSelector::launch(const BoardWorld& world, Packet packet, Vector gravity,
                                                      Context context) {
    core::Selector next = core;
    auto requests_r = next.begin_launch(packet, gravity, settings.query);
    if (requests_r.is_err()) {
        std::ostringstream os;
        os << "BipedAir launch producer rejected; no packet fields substituted gravity=[" << fmt::debug_f32(gravity[0])
           << ", " << fmt::debug_f32(gravity[1]) << ", " << fmt::debug_f32(gravity[2]) << ", "
           << fmt::debug_f32(gravity[3]) << "] settings=Settings { height: " << fmt::debug_f32(settings.query.height)
           << ", sphere_radius: " << fmt::debug_f32(settings.query.sphere_radius)
           << ", start_index: " << settings.query.start_index << " } error=" << requests_r.unwrap_err();
        log::error(os.str());  // SIMREF
        return rs::make_err(requests_r.unwrap_err());
    }
    auto scene_r = contact_toolkit::StaticScene::new_(world);
    if (scene_r.is_err()) return rs::make_err(scene_r.unwrap_err());
    const auto& scene = scene_r.unwrap();
    std::vector<QueryResult> results;
    for (const auto& request : requests_r.unwrap()) {
        auto r = scene.trajectory(request, context.matching_group_2952, core::MESH_REJECT_MASK);
        if (r.is_err()) return rs::make_err(r.unwrap_err());
        results.push_back(r.unwrap());
    }
    core = std::move(next);
    completed_launch = std::move(results);
    completed_requery = std::nullopt;
    return rs::Unit{};
}

rs::Result<std::optional<std::size_t>, const char*> AirSelector::consume_launch(const BoardWorld& world,
                                                                                Context context,
                                                                                float deck_half_wheelbase) {
    if (!completed_launch.has_value()) return std::optional<std::size_t>{};
    const auto& results = *completed_launch;
    if (!core.sampling.pending_8492) return std::optional<std::size_t>{};
    core::Selector next = core;
    {
        auto r = next.observe_launch(results);
        if (r.is_err()) return rs::make_err(r.unwrap_err());
    }
    S3_ASSERT(!next.candidates.empty() && !next.predictions.empty());
    const auto first = next.candidates[0];
    const auto prediction = next.predictions[0];
    std::optional<ledge::Adjustment> adjustment;
    if (const auto search = ledge::search(first, prediction, context)) {
        auto edges_r = ground_query::with_world_scene<std::vector<Edge>>(
            world, ground_query::PrimaryEdges::Normal{}, sc::physics::Slice<const ground_query::IndexedEdgeBody>{},
            [&](ground_query::world::WorldScene& scene) { return scene.edge_candidates(*search); });
        if (edges_r.is_err()) return rs::make_err(edges_r.unwrap_err());
        const auto filtered = ledge::filter_edges(edges_r.unwrap(), first.trajectory.position);
        if (const auto chosen = ledge::choose(first, prediction, context, settings.query.sphere_radius, filtered)) {
            if (!rs::is_finite(deck_half_wheelbase) || deck_half_wheelbase <= 0.0f) {
                return rs::make_err("BipedAir ledge requires actual positive board half-wheelbase");
            }
            if (const auto lines = ledge::lines(*chosen, deck_half_wheelbase)) {
                auto scene_r = contact_toolkit::StaticScene::new_(world);
                if (scene_r.is_err()) return rs::make_err(scene_r.unwrap_err());
                const std::vector<Line> requests(lines->begin(), lines->end());
                auto hits = scene_r.unwrap().lines(requests, context.matching_group_2952);
                if (hits.is_err()) return rs::make_err(hits.unwrap_err());
                auto consumed = ledge::consume_lines(hits.unwrap());
                if (consumed.is_err()) return rs::make_err(consumed.unwrap_err());
                adjustment = chosen;
            }
        }
    }
    auto index = next.select_launch(context, first, adjustment);
    if (index.is_err()) return rs::make_err(index.unwrap_err());
    core = std::move(next);
    completed_launch = std::nullopt;
    return std::optional<std::size_t>(index.unwrap());
}

void AirSelector::adjust_animation(std::int32_t frame, Vector animation, Frame axes) {
    core.adjust_animation(frame, animation, axes, settings.query.sphere_radius);
}

void AirSelector::sample(std::int32_t frame, float dt, core::TrajectoryResult& out) {
    core.sample(frame, dt, settings.blend, out);
}

rs::Result<bool, const char*> AirSelector::requery(const BoardWorld& world, Context context, std::uint32_t flags_2472,
                                                   std::uint32_t flags_2488) {
    core::Selector next = core;
    auto request_r = next.begin_requery(flags_2472, flags_2488, settings.query.sphere_radius);
    if (request_r.is_err()) return rs::make_err(request_r.unwrap_err());
    if (!request_r.unwrap().has_value()) return false;
    const auto request = *request_r.unwrap();
    auto scene_r = contact_toolkit::StaticScene::new_(world);
    if (scene_r.is_err()) return rs::make_err(scene_r.unwrap_err());
    auto result_r = scene_r.unwrap().trajectory(request, context.matching_group_2952, core::MESH_REJECT_MASK);
    if (result_r.is_err()) return rs::make_err(result_r.unwrap_err());
    const QueryResult result = result_r.unwrap();
    core = std::move(next);
    completed_requery = Prediction{result, request};
    if (completed_launch.has_value() && !completed_launch->empty()) completed_launch->front() = result;
    if (!core.predictions.empty()) core.predictions.front() = Prediction{result, request};
    return true;
}

rs::Result<bool, const char*> AirSelector::consume_requery() {
    if (!completed_requery.has_value()) return false;
    Prediction prediction = *completed_requery;
    //CC50 precedesE8A0 and may shift slot0's trajectory/contact time.
    if (!core.predictions.empty()) prediction = core.predictions.front();
    auto r = core.complete_requery(prediction);
    if (r.is_err()) return rs::make_err(r.unwrap_err());
    completed_requery = std::nullopt;
    return true;
}

rs::Result<std::optional<std::size_t>, const char*> AirSelector::consume(const BoardWorld& world, Context context,
                                                                         float deck_half_wheelbase) {
    core.sampling.preinitialized_8494 = false;
    std::optional<std::size_t> selected;
    if (core.sampling.pending_8492) {
        auto r = consume_launch(world, context, deck_half_wheelbase);
        if (r.is_err()) return rs::make_err(r.unwrap_err());
        selected = r.unwrap();
    }
    if (core.requery_pending_8499) {
        auto r = consume_requery();
        if (r.is_err()) return rs::make_err(r.unwrap_err());
    }
    core.sampling.restart_allowed_8493 = true;
    return selected;
}

void AirSelector::exit() { core.exit(); }

}  // namespace s3::game::physics::offboard::air_selector
