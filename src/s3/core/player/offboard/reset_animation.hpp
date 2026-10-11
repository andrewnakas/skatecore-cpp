// Port of skate-core/src/player/offboard/reset_animation.rs (whole file).
// Original TU3 reset field writes. Borrow the canonical animation/MG owners.
#pragma once
#include <array>
#include <cstdint>

namespace s3::player::offboard::reset_animation {

/// 82B97308 ->82B972A8. This is the original stance enum comparison, not XOR
/// of two guessed booleans; all original integer cases remain distinguishable.
inline void given_stance(std::uint32_t original_stance, std::uint32_t& requested, std::uint32_t& flags, bool& mirrored) {
    if ((original_stance != 1 || requested != 0) && (original_stance != 0 || requested != 1)) {
        flags |= 0xc0000000u;
        mirrored = original_stance != 0;
    } else {
        flags &= 0x3fffffffu;
        mirrored = original_stance != 1;
    }
    requested = 0;
}
/// 82B98050's two raw rlwinm masks; unrelated stance/request fields survive.
inline void skater_flags(std::uint32_t& flags, bool& mirrored, std::uint32_t& word_15320) {
    flags &= 0x0ff7ffffu;
    mirrored = false;
    word_15320 = 0;
}
/// FullMG5840..5920 inclusive, borrowed as its21 published scalar/flag words.
/// Only the recovered writes occur. In particular5912/5916 survive.
inline void motion_output(std::array<std::uint32_t, 21>& words, std::uint32_t& gesture,
                          std::array<std::uint8_t, 4>& gesture_flags) {
    for (std::size_t index : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 17}) words[index] = 0;
    words[11] &= 0x7fffffffu;
    words[13] &= 0x1fffffffu;
    words[15] &= 0x1fffffffu;
    words[16] &= 0x3fffffffu;
    words[20] = (words[20] & 0x01fbffffu) | 0x02000000u;
    gesture = 37;
    gesture_flags[0] = 0;
    gesture_flags[1] = 0;
}
/// 8258F360 writes four meaningful request words and clears last word bit31.
/// The native last word's lower31 bits originate in uninitialized stack data;
/// they are not assigned a fabricated deterministic payload by this API.
inline void motion_request(std::array<std::uint32_t, 4>& words_2664_2676, bool& active_2680) {
    words_2664_2676 = {0, 0x3e4ccccdu, 0, 0};
    active_2680 = false;
}

}  // namespace s3::player::offboard::reset_animation
