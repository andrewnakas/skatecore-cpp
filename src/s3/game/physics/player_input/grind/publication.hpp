// Port of skate-game/src/physics/player_input/grind/publication.rs.
#pragma once
#include <optional>
#include <string>

#include "s3/game/physics/player_input/grind.hpp"

namespace s3::game::physics::player_input::grind::publication {

/// `pub(super)`.
void surface(GrindInvestigationFields& fields, const GrindSurface& surface);

/// `pub(super)`.
rs::Result<ManagerObservation, std::string> observation(const ProcessedPhysicsInput& p, PostContext c,
                                                        std::optional<PrimitiveMetadata> metadata,
                                                        const manager::Jumper& jumper);

// Private `fn family(u32) -> Result<Family, String>` lives in publication.cpp (needs physics/grind.hpp).

}  // namespace s3::game::physics::player_input::grind::publication
