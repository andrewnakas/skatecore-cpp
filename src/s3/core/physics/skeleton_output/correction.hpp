// Port of skate-core/src/physics/skeleton_output/correction.rs.
// PostWipeoutCheck82BD83E0's error correction before wobble and physical IK.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/skeleton_body.hpp"

namespace s3::physics::skeleton_output::correction {

using skeleton_body::SkeletonBody;

/// Skeleton16224 is the already-produced board prediction error, and16388 says the
/// record adjustment remains pending. This function does not synthesize errors.
void apply(SkeletonBody& body, bool& pending, std::array<float, 4> error,
           std::array<float, 4> ground_normal, bool wipeout, std::uint32_t flags_2468,
           std::uint32_t flags_2476);

struct CorrectionState {
    /// Skeleton16224, initialized zero at82BD7918.
    std::array<float, 4> board_prediction_error{};
    /// Skeleton16388, initialized false at82BD7998 and set by Ground::Update.
    bool pending = false;

    /// UpdatePostPhysics82BD8194..81B0, before collision and error feedback:
    /// actual board GetPartTransform translation minus predicted16112.
    void observe_board(std::array<float, 4> actual, std::array<float, 4> predicted) {
        for (std::size_t i = 0; i < 4; ++i) board_prediction_error[i] = actual[i] - predicted[i];
    }
    void apply(SkeletonBody& body, std::array<float, 4> ground_normal, bool wipeout,
               std::uint32_t flags_2468, std::uint32_t flags_2476) {
        correction::apply(body, pending, board_prediction_error, ground_normal, wipeout, flags_2468,
                          flags_2476);
    }
};

}  // namespace s3::physics::skeleton_output::correction
