// Port of skate-game/src/physics/riding_outputs/probes.rs.
//! The fifth/sixth board probes, kept in the same query/publication phase as
//! the wheel probes. Geometry is the world's recovered triangle dispatcher.
#include "s3/game/physics/riding_outputs/probes.hpp"

#include "s3/core/physics/board.hpp"

namespace s3::game::physics::riding_outputs::probes {

using sc::math::Vector3;
namespace board_probes = sc::physics::board_probes;

void BoardProbes::reset_results() {
    deck = board_probes::BoardProbeState{};
    wall = board_probes::BoardProbeState{};
}

sc::riding::grounded::state::board_types::GroundContactFrame BoardProbes::wall_contact_frame(float time2752,
                                                                                            float time2756) const {
    auto v = [](Vector3 p) { return std::array<float, 4>{p.x, p.y, p.z, 0.0f}; };
    sc::riding::grounded::state::board_types::GroundContactFrame out{};
    out.vector_8032 = v(wall.start_);
    out.vector_8048 = v(wall.point);
    out.vector_8064 = v(wall.normal);
    out.word_8080 = wall.surface_tag;
    out.flag_8084 = wall.hit;
    out.scalar_2752 = time2752;
    out.scalar_2756 = time2756;
    return out;
}

void BoardProbes::prepare_wall(const sc::player::input_phase::ProcessedPhysicsInput& input,
                               const sc::physics::board_toolkit::BoardToolkit& toolkit) {
    auto vector = [](std::array<float, 4> value) { return Vector3::new_(value[0], value[1], value[2]); };
    board_probes::WallLineInput line{};
    line.state = input.state_2508;
    line.contact_normal = vector(rs::from_bits(input.vectors_464_480_496_512_528[0]));
    line.skater_up = vector(rs::from_bits(input.vectors_544_560_592_608[0]));
    line.deck_position = vector(toolkit.deck[3]);
    line.time = input.time_on_ground_2752;
    wall_line = board_probes::wall_probe(line);
}

Status BoardProbes::start(const sc::physics::board_runtime::BoardRuntime& board,
                          const sc::physics::board_world::BoardWorld& world) {
    if (pending.has_value()) return rs::make_err(std::string("Board probes submitted twice without publication"));
    const auto line = board_probes::deck_probe(
        board.part_transforms()[sc::physics::board::index(sc::physics::board::BodyId::Deck)].translation);
    deck.start(line.start);
    const auto deck_r = world.query_swept_line(line.start, line.end, board_probes::DECK_PROBE_RADIUS);
    if (deck_r.is_err()) return rs::make_err(std::string("Stock deck ground probe: ") + deck_r.unwrap_err());
    std::optional<board_probes::BoardProbeHit> deck_hit;
    if (deck_r.unwrap()) deck_hit = hit(*deck_r.unwrap());
    std::optional<std::optional<board_probes::BoardProbeHit>> wall_hit;
    if (wall_line.has_value()) {
        const auto l = *wall_line;
        wall.start(l.start);
        const auto wall_r = world.query_thin_line(l.start, l.end);
        if (wall_r.is_err()) return rs::make_err(std::string("Stock wall floor probe: ") + wall_r.unwrap_err());
        std::optional<board_probes::BoardProbeHit> h;
        if (wall_r.unwrap()) h = hit(*wall_r.unwrap());
        wall_hit = h;
    } else {
        wall.disable();
        wall_hit = std::nullopt;
    }
    pending = Pending{deck_hit, wall_hit};
    return ok();
}

Status BoardProbes::publish() {
    if (!pending.has_value())
        return rs::make_err(std::string("Board probe publication requires a submitted batch"));
    const Pending p = *pending;
    pending = std::nullopt;
    deck.publish(p.deck);
    if (p.wall.has_value()) {
        wall.publish(*p.wall);
    } else {
        wall.disable();
    }
    return ok();
}

board_probes::BoardProbeHit hit(sc::physics::board_world::WorldLineHit h) {
    return board_probes::BoardProbeHit{h.geometry.position, h.geometry.normal, h.tag};
}

}  // namespace s3::game::physics::riding_outputs::probes
