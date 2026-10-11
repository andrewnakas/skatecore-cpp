// Port of skate-core/src/player/offboard/biped_air.rs (legacy runtime) and its
// `pub mod recovered` (biped_air/recovered.hpp).
// Pre-migration runtime restored for the Ground build checkpoint.
// Its known Air lifecycle defects are NOT repaired or validated by this rollback.
// The separately compiled recovered module is the unfinished replacement.
// Persistent BipedAir owner storage and lifecycle publication.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/player/offboard/air_launch.hpp"
#include "s3/core/player/offboard/biped_air/recovered.hpp"
#include "s3/core/player/offboard/cadence.hpp"
#include "s3/core/player/offboard/controller.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::player::offboard::biped_air {

using Vector = std::array<float, 4>;
using Frame = std::array<Vector, 4>;
using air_launch::Packet;

struct Input {
    Vector position_592{};
    Vector velocity_608{};
    Vector up_544{};
    Vector forward_224{};
    Vector board_position_112{};
    std::optional<air_launch::DepartureGeometry> departure_geometry;
    Vector velocity_912{};
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t previous_state_2504 = 0;
    std::uint32_t current_state_2508 = 0;
    std::uint32_t current_category_2512 = 0;
    std::uint32_t previous_category_2516 = 0;
    float raw_x_2692 = 0.0f;
    float raw_z_2688 = 0.0f;
    bool current = false;
};
S3_DERIVE_EQ(Input, position_592, velocity_608, up_544, forward_224, board_position_112, departure_geometry,
             velocity_912, flags_2472, flags_2476, flags_2480, previous_state_2504, current_state_2508,
             current_category_2512, previous_category_2516, raw_x_2692, raw_z_2688, current)

struct Output {
    Packet packet{};
    Vector position_288{};
    Vector velocity_304{};
    float phase_444 = 0.0f;
    float phase_counter_448 = 0.0f;
    std::uint32_t update_count_452 = 0;
    bool landed_404 = false;
    bool wipeout_547 = false;
};
S3_DERIVE_EQ(Output, packet, position_288, velocity_304, phase_444, phase_counter_448, update_count_452, landed_404,
             wipeout_547)

struct State {
    Packet packet = Packet::initialized(0.0f);
    cadence::BipedCadence cadence{};
    Vector position_288{};
    Vector velocity_304{};
    float phase_444 = 0.0f;
    float phase_counter_448 = 0.0f;
    std::uint32_t update_count_452 = 0;
    bool landed_404 = false;
    bool wipeout_547 = false;
    bool active = false;

    ///82D2E5C0: clear the retained landing/output flags and enter with a
    ///fresh launch packet.  The caller supplies the actual retained scalar.
    void enter(float retained_packet_scalar_104, Vector position) {
        packet = Packet::initialized(retained_packet_scalar_104);
        position_288 = position;
        velocity_304 = Vector{};
        phase_444 = 0.0f;
        phase_counter_448 = 0.0f;
        update_count_452 = 0;
        landed_404 = false;
        wipeout_547 = false;
        active = true;
    }
    ///82D2EF20: the native exit clears the trajectory selector and leaves no
    ///live BipedAir publication behind.
    void exit() {
        active = false;
        landed_404 = false;
    }
    rs::Result<rs::Unit, const char*> update(const Input& input, const controller::State& controller,
                                             const point_graph::PointGraph<8>& turn_vs_speed,
                                             air_launch::Settings settings) {
        if (!active) {
            return rs::Unit{};
        }
        air_launch::Processed processed;
        processed.board_position_112 = input.board_position_112;
        processed.forward_224 = input.forward_224;
        processed.up_544 = input.up_544;
        processed.position_592 = input.position_592;
        processed.velocity_608 = input.velocity_608;
        processed.velocity_912 = input.velocity_912;
        processed.departure_geometry = input.departure_geometry;
        processed.flags_2472 = input.flags_2472;
        processed.flags_2476 = input.flags_2476;
        processed.flags_2480 = input.flags_2480;
        processed.previous_state_2504 = input.previous_state_2504;
        processed.current_state_2508 = input.current_state_2508;
        processed.current_category_2512 = input.current_category_2512;
        processed.previous_category_2516 = input.previous_category_2516;
        processed.raw_x_2692 = input.raw_x_2692;
        processed.raw_z_2688 = input.raw_z_2688;
        auto r = air_launch::produce(packet, controller, turn_vs_speed, settings, processed, input.current);
        if (r.is_err()) return rs::make_err(r.unwrap_err());
        position_288 = packet.position_32;
        velocity_304 = packet.velocity_0;
        phase_counter_448 += rs::from_bits(0x3c888889u);
        update_count_452 = update_count_452 + 1u;
        return rs::Unit{};
    }
    Output output() const {
        Output o;
        o.packet = packet;
        o.position_288 = position_288;
        o.velocity_304 = velocity_304;
        o.phase_444 = phase_444;
        o.phase_counter_448 = phase_counter_448;
        o.update_count_452 = update_count_452;
        o.landed_404 = landed_404;
        o.wipeout_547 = wipeout_547;
        return o;
    }
};

}  // namespace s3::player::offboard::biped_air
