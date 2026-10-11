// Port of skate-game/src/physics/offboard/landing_deck.rs
//! Offboard host for original LandingOnDeckManager82D78B28..82D79E30.
//! One retained manager and real scene-query completion; no physical-state facade.
#pragma once
#include <optional>
#include <string>

#include "s3/core/player/input_phase.hpp"
#include "s3/core/player/offboard/landing_deck.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/offboard/contact_toolkit.hpp"

namespace s3::game::physics::animated_skeleton {
struct AnimatedSkeleton;  // physics/animated_skeleton.hpp (group D)
}
namespace s3::game::physics::player_input {
struct PlayerInputRuntime;  // physics/player_input.hpp (group C)
}

namespace s3::game::physics::offboard::landing_deck {

namespace native = ::s3::player::offboard::landing_deck;
using contact_toolkit::StaticScene;
using native::AssistInput;
using native::IkInput;
using native::Manager;
using native::QueryResult;
using native::Settings;
using native::UpdateOutput;
using native::Vector;
using physics::animated_skeleton::AnimatedSkeleton;
using physics::player_input::PlayerInputRuntime;

struct PublishTargets;  // output.hpp

struct Owner {
    ///Air and transitions must mutate this same manager, including Reset/force.
    Manager manager{};
    Settings settings{};
    ///Some is an actually executed query, including an actual miss.
    ///Retained until the ordered PostPhysics phase, never a fabricated seed.
    // Private in Rust.
    std::optional<QueryResult> completion;

    static rs::Result<Owner, std::string> load(const sd::Collections& data);
    ///82D78B28 deliberately retains proposed trajectory AND pending completion.
    void reset();
    ///82D78D30. Query failure propagates without committing the speculative state.
    Status assist(const StaticScene& scene, const PlayerInputRuntime& input, float maximum_velocity_change);
    ///82D794A0: returns EVERY numeric output. A returned query has already run;
    ///the caller must not submit UpdateOutput.query a second time.
    rs::Result<UpdateOutput, std::string> update(const StaticScene& scene, const PlayerInputRuntime& input);
    ///82D792D0. mapped_position_12608 is the PRE-adjustment mapped board
    ///translation copied by82BD8CB0..8D14, not animation_board after offsets.
    ///The shared pose producer supplies that retained value explicitly.
    rs::Result<Vector, std::string> calculate_accurate_ik_offset(const PlayerInputRuntime& input,
                                                                 const AnimatedSkeleton& animation,
                                                                 Vector mapped_position_12608, float time_to_land);
    native::FillOutput fill() const;

    // ---- output.rs ----
    void publish(PublishTargets output) const;

    // ---- query.rs ----
    ///Run the actual query before acknowledging native pending262.
    ///The returned real result remains private until PostPhysics.
    Status submit(const StaticScene& scene, native::QueryRequest request,
                  const sc::player::input_phase::ProcessedPhysicsInput& processed);
    ///Native caller tests262 before Sync. Host completes work synchronously at
    ///submission but preserves its consumption phase; this is not a claim of
    ///the console job backend's timing or allocation semantics.
    Status post_physics(const PlayerInputRuntime& input);
};

}  // namespace s3::game::physics::offboard::landing_deck

#include "s3/game/physics/offboard/landing_deck/input.hpp"
#include "s3/game/physics/offboard/landing_deck/output.hpp"
#include "s3/game/physics/offboard/landing_deck/query.hpp"
