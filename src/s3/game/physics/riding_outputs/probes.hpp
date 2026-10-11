// Port of skate-game/src/physics/riding_outputs/probes.rs.
//! The fifth/sixth board probes, kept in the same query/publication phase as
//! the wheel probes. Geometry is the world's recovered triangle dispatcher.
#pragma once
#include <optional>
#include <string>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_ground.hpp"
#include "s3/core/physics/board_probes.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/core/riding/grounded/state/board_types.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::riding_outputs::probes {

/// Private in Rust.
struct Pending {
    std::optional<sc::physics::board_probes::BoardProbeHit> deck;
    std::optional<std::optional<sc::physics::board_probes::BoardProbeHit>> wall;
};

/// `impl Default`: Skateboard ctor82C0111C starts optional wall line byte291 clear.
struct BoardProbes {
    sc::physics::board_probes::BoardProbeState deck{};
    sc::physics::board_probes::BoardProbeState wall{};
    // Private in Rust.
    std::optional<sc::physics::board_ground::WheelLine> wall_line;
    std::optional<Pending> pending;

    static BoardProbes default_() { return BoardProbes{}; }

    ///ResetBoardBody82C0D680 clears published probe records, preserving
    ///query handles204C/2050 and the wall-query enable bit.
    void reset_results();
    sc::riding::grounded::state::board_types::GroundContactFrame wall_contact_frame(float time2752,
                                                                                   float time2756) const;
    /// PostPhysics82C02138 calls82C01F10 before updating board contact output.
    /// Its saved endpoints and enabled291 feed the NEXT StartBoard82DB6310.
    /// Processed112 is the input phase's cached deck translation, published by
    ///82C01460, not the newly solved body's current transform.
    void prepare_wall(const sc::player::input_phase::ProcessedPhysicsInput& input,
                      const sc::physics::board_toolkit::BoardToolkit& toolkit);
    Status start(const sc::physics::board_runtime::BoardRuntime& board,
                 const sc::physics::board_world::BoardWorld& world);
    Status publish();
};

/// Private in Rust.
sc::physics::board_probes::BoardProbeHit hit(sc::physics::board_world::WorldLineHit hit);

}  // namespace s3::game::physics::riding_outputs::probes
