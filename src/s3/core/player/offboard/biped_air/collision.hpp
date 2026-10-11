// Port of skate-core/src/player/offboard/biped_air/collision.rs (types).
// Air-owned response82D2F748; never changes the shared contact/joint solver.
// State methods are defined in recovered.cpp.
#pragma once
#include <array>
#include <optional>

namespace s3::player::offboard::biped_air::recovered::collision {

using Vector = std::array<float, 4>;

struct Response {
    ///82D2F93C..950: byte52/value184/requestcounter200.
    bool request_52 = false;
    std::optional<Vector> restart;
};

}  // namespace s3::player::offboard::biped_air::recovered::collision
