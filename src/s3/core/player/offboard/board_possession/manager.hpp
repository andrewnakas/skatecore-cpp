// Port of skate-core/src/player/offboard/board_possession/manager.rs (whole file).
// Ground state56's hand manager, distinct from SkateboardController448.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::offboard::board_possession::manager {

using Vector = std::array<float, 4>;

struct Hand {
    std::array<Vector, 5> vectors_0_to_64{
        Vector{0.0f, 0.0f, 0.0f, 0.0f}, Vector{0.0f, 0.0f, 0.0f, 0.0f}, Vector{0.0f, 1.0f, 0.0f, 0.0f},
        Vector{0.0f, 0.0f, 0.0f, 0.0f}, Vector{0.0f, 0.0f, 0.0f, 0.0f}};
    std::uint32_t word_80 = 0;
    bool flag_84 = false;
    std::array<float, 2> scalars_96_100{};
    std::array<bool, 4> flags_104_to_107{};
};

/// Actual skeleton16432 object's reset fields; all other fields survive.
struct SkeletonReset {
    bool* enabled_464;
    std::array<float, 4>* values_468;
    std::array<float, 4>* values_484;
    std::array<std::uint32_t, 4>* words_500;
};

struct State {
    std::array<Hand, 2> hands{};
    std::array<Vector, 4> vectors_224_to_272{};
    std::array<float, 3> scalars_288_to_296{};
    std::uint32_t word_300 = 2;
    std::array<bool, 4> flags_304_to_307{};
    std::array<std::uint32_t, 3> words_308_to_316{};

    ///82D77168 writes only the represented fields, not pointer fields320 onward
    ///or unwritten gaps88..95 in each hand record.
    void reset() { *this = State{}; }
    ///82D770D8 preserves hand state across500/501/502 and always publishes304.
    void enter(std::uint32_t previous_2504, std::uint32_t current_2508, SkeletonReset skeleton) {
        if (!(previous_2504 == 500 || previous_2504 == 501 || previous_2504 == 502)) {
            reset();
            *skeleton.values_468 = {0.0f, 0.0f, 0.0f, 0.0f};
            *skeleton.values_484 = {0.0f, 0.0f, 0.0f, 0.0f};
            *skeleton.words_500 = {0, 0, 0, 0};
            *skeleton.enabled_464 = false;
        }
        flags_304_to_307[0] = current_2508 == 502;
    }
};

}  // namespace s3::player::offboard::board_possession::manager
