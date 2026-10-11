// Port of skate-game/src/physics/grind.rs: `enum Family` (+ impl Family).
// Split out of grind.hpp so the grind submodule headers can use it without
// including grind.hpp (which includes every submodule).
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>

namespace s3::game::physics::grind {

/// Physical/output family, NOT the seven-valued admission kind.
enum class Family : std::uint32_t {
    FiftyFifty = 0,
    Boardslide = 1,
    Tipslide = 2,
    FiveO = 3,
    Backslash = 4,
    Darkslide = 5,
};

/// `Family::from_physical_state`.
inline std::optional<Family> from_physical_state(std::uint32_t state) {
    switch (state) {
        case 400: return Family::Boardslide;
        case 401: return Family::FiftyFifty;
        case 402: return Family::Tipslide;
        case 403: return Family::FiveO;
        case 404: return Family::Backslash;
        case 405: return Family::Darkslide;
        default: return std::nullopt;
    }
}
/// `Family::physical_state`.
inline std::uint32_t physical_state(Family family) {
    switch (family) {
        case Family::Boardslide: return 400;
        case Family::FiftyFifty: return 401;
        case Family::Tipslide: return 402;
        case Family::FiveO: return 403;
        case Family::Backslash: return 404;
        case Family::Darkslide: return 405;
    }
    return 0;
}
/// `family as usize` / `family as u32`.
inline std::size_t index(Family family) { return static_cast<std::size_t>(family); }

}  // namespace s3::game::physics::grind
