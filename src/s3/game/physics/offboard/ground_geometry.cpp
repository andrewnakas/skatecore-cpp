// Port of skate-game/src/physics/offboard/ground_geometry.rs
#include "s3/game/physics/offboard/ground_geometry.hpp"

namespace s3::game::physics::offboard::ground_geometry {

using ground_sync::SceneService;

rs::Result<State, std::string> State::load(const sd::Collections& data) {
    State s{};
    s.pending = std::nullopt;
    //82C209E4 and Skate2 GetGeometryCollisionOffset82CFABD8:
    //physics_grinds layout636, full keyCBFEFFEA12F4CC27.
    S3G_TRY(s.collision_offset, data.float_("physics_grinds", "default", "DeckCenterToTruck"));
    return s;
}

void State::reset() { pending = std::nullopt; }

GroundAdjustment State::consume(ConsumeInput input) {
    std::optional<query::GroundGeometry> geometry;
    if (pending.has_value()) {
        const auto taken = std::move(*pending);
        pending.reset();
        geometry = query::interpret_hits(taken.first, taken.second);
    }
    return query::consume_geometry(input, geometry);
}

Status State::submit(const BoardWorld& world, std::array<std::array<float, 4>, 4> frame_, std::array<float, 4> velocity,
                     QueryContext context, std::uint32_t flags_2488) {
    const Frame frame = query_frame(frame_);
    const auto search = query::edge_search(frame, context, xyz(velocity), flags_2488);
    SceneService service(&world);
    auto candidates = service.edge_candidates(search);
    if (candidates.is_err()) return rs::make_err(std::string(candidates.unwrap_err()));
    std::optional<GroundQueryPacket> packet;
    if (const auto edge = query::select_edge(search, candidates.unwrap())) {
        packet = query::prepare_packet(frame, context, *edge, collision_offset);
    }
    if (packet.has_value()) {
        //82C20BF0 starts the real batch during Sync. Host execution may be
        //synchronous, but interpretation/publication waits for PreUpdate.
        SceneService lines_service(&world);
        auto hits = lines_service.query_lines(*packet);
        if (hits.is_err()) return rs::make_err(std::string(hits.unwrap_err()));
        pending = std::make_pair(*packet, hits.unwrap());
    }
    //82D321CC: no selected edge means no submit, not a fabricated query.
    return ok();
}

Frame query_frame(std::array<std::array<float, 4>, 4> frame) {
    return Frame{xyz(frame[0]), xyz(frame[1]), xyz(frame[2]), xyz(frame[3])};
}
Vector3 xyz(std::array<float, 4> v) { return Vector3::new_(v[0], v[1], v[2]); }
std::array<float, 4> lanes(Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }
std::array<std::array<float, 4>, 4> native_frame(Frame frame) {
    return {lanes(frame.right), lanes(frame.up), lanes(frame.forward), lanes(frame.position)};
}

}  // namespace s3::game::physics::offboard::ground_geometry
