// Port of skate-game/src/physics/grind/rng.rs.
#include "s3/game/physics/grind/rng.hpp"

namespace s3::game::physics::grind::rng {

OrientationRandom OrientationRandom::new_() {
    //Use the existing recovered MotionRandom initialization, not an OS
    //seed or a fabricated substitute algorithm. Seed words821642B0.
    OrientationRandom r;
    r.words = {0, 0, 0xf22d0e56u, 0x883126e9u, 0xc624dd2fu, 0x0702c49cu, 0x9e353f7du, 0x6fdf3b64u};
    return r;
}

///82D405C8/E0/F0: exactly three successive unsigned draws, in call order.
///The existing noise leaf owns modulo100000 and angle mapping.
std::array<std::uint32_t, 3> OrientationRandom::take_three() {
    const std::uint32_t a = next_u32();
    const std::uint32_t b = next_u32();
    const std::uint32_t c = next_u32();
    return {a, b, c};
}

std::uint32_t OrientationRandom::next_u32() {
    const std::uint32_t last = words[7];
    const std::uint32_t previous = words[6];
    std::uint32_t sum = previous + last;
    std::uint32_t carry = (sum < last || sum < previous) ? 1u : 0u;
    words[6] = sum;
    for (std::size_t index = 5; index >= 2; --index) {
        const std::uint32_t prev = words[index];
        sum = prev + sum + carry;
        carry = sum < prev ? 1u : 0u;
        words[index] = sum;
    }
    words[7] = last + 1u;
    if (words[7] == 0) {
        for (std::size_t index = 6; index >= 2; --index) {
            words[index] = words[index] + 1u;
            if (words[index] != 0) {
                break;
            }
        }
    }
    words[1] = words[1] + 1u;
    return words[2];
}

}  // namespace s3::game::physics::grind::rng
