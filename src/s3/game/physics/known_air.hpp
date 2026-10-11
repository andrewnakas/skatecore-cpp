// Port of skate-game/src/physics/known_air.rs.
//! Native KnownAir201 lifecycle82D352D0/82D35C58/82D35920/82D36590/82D36880.
//! All phase adapters borrow the production owners; the state copy below is a
//! stack-local value while those owners are borrowed, then published once.
#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "s3/core/air/known.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/known_air/flip.hpp"
#include "s3/game/physics/known_air/math.hpp"
#include "s3/game/physics/known_air/output.hpp"
#include "s3/game/physics/known_air/runtime.hpp"
#include "s3/game/physics/known_air/settings.hpp"

namespace s3::game::physics::known_air {

using ::s3::air::known::KnownAirFrame;
using ::s3::air::known::KnownAirModeSettings;
using ::s3::air::known::KnownAirOutput;
using ::s3::air::known::KnownAirReckoningFields;
using ::s3::air::known::KnownAirSettings;
using ::s3::air::known::KnownAirState;
using ::s3::air::known::KnownAirWipeoutRequest;
using ::s3::air::known::KnownAirWipeoutSettings;

struct KnownAir {
    KnownAirState state{};
    // Private in Rust.
    KnownAirSettings settings{};
    std::array<KnownAirModeSettings, 5> modes{};
    KnownAirWipeoutSettings wipeout{};
    std::array<float, 4> flip_axis_adjustment{};

    static rs::Result<KnownAir, std::string> load(const sd::Collections& data);
    // Private in Rust.
    rs::Result<KnownAirModeSettings, std::string> mode(std::uint32_t index) const;
};

// Private in Rust.
rs::Result<KnownAirFrame, std::string> frame(const SkaterRuntime& skater, std::uint32_t next);
KnownAirReckoningFields reckoning(const GamePhysics& physics, const SkaterRuntime& skater);

Status enter(GamePhysics& physics, SkaterRuntime& skater);
Status update(GamePhysics& physics, SkaterRuntime& skater);
Status exit(GamePhysics& physics, SkaterRuntime& skater, std::uint32_t next);
///82D36590: apex observation, shared air-wipeout(true), then upside-down request.
Status post_physics(GamePhysics& physics, SkaterRuntime& skater);
rs::Result<KnownAirOutput, std::string> fill(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::known_air
