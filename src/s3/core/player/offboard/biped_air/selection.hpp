// Port of skate-core/src/player/offboard/biped_air/selection.rs.
// Offboard selector commit82D6CC50, after scoring82D6D020.
// The SelectorState methods (reset_sampling, seed_fallback, commit) are declared
// in sampling.hpp and defined in sampling.cpp.
#pragma once
#include <cstddef>
#include <optional>
#include <vector>

#include "s3/core/player/offboard/biped_air/sampling.hpp"

namespace s3::player::offboard::biped_air::recovered::selection {

using air::trajectory::Trajectory;
using Vector = std::array<float, 4>;

Trajectory cleared_trajectory();

///Candidate4032+128*i. The trajectory occupies0..63;100 is contact-position Y.
struct Candidate {
    Trajectory trajectory{};
    Vector normal_64{};
    Vector contact_velocity_80{};
    Vector contact_position_96{};
    std::int32_t start_frame_112 = 0;
    std::int32_t landing_frame_116 = 0;
    bool valid_120 = false;
    bool special_121 = false;

    ///82D6BD78 clears every candidate, independently of the launch packet.
    static Candidate reset() {
        Candidate c;
        c.trajectory = cleared_trajectory();
        return c;
    }
};

///82D6CC88..CCBC uses a strict comparison; equal scores keep the earlier index.
///The native caller always supplies at least one candidate (packet108 >=1).
std::optional<std::size_t> selected_index(const float* scores, std::size_t count);
inline std::optional<std::size_t> selected_index(const std::vector<float>& scores) {
    return selected_index(scores.data(), scores.size());
}

}  // namespace s3::player::offboard::biped_air::recovered::selection
