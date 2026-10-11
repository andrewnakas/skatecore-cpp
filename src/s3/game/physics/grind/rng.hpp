// Port of skate-game/src/physics/grind/rng.rs.
//! Host-owned orientation stream using recovered82970628 arithmetic.
//! Draw ownership/seed bookkeeping is host policy; do not claim original
//! cross-subsystem interleaving. No random draw occurs until AddNoise asks.
#pragma once
#include <array>
#include <cstdint>

namespace s3::game::physics::grind::rng {

struct OrientationRandom {
    static OrientationRandom new_();
    ///82D405C8/E0/F0: exactly three successive unsigned draws, in call order.
    ///The existing noise leaf owns modulo100000 and angle mapping.
    std::array<std::uint32_t, 3> take_three();

    std::uint32_t next_u32();  // private in Rust

    std::array<std::uint32_t, 8> words{};
};

}  // namespace s3::game::physics::grind::rng
