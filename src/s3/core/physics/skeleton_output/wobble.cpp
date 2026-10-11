// Port of skate-core/src/physics/skeleton_output/wobble.rs.
#include "s3/core/physics/skeleton_output/wobble.hpp"

#include "s3/core/trigonometry.hpp"

namespace s3::physics::skeleton_output::wobble {

void apply(Output output, skeleton_animation_record::AnimationPartTransform& board) {
    if (!output.sampled) {
        return;
    }
    using skeleton_animation_record::compose_affine;
    const auto [sine, cosine] = trigonometry::sin_cos(output.tilt);
    // 822FB890 permute and vrlimi mask2 retain the native packed W lanes.
    const skeleton_animation_record::AnimationPartTransform rotation{{
        {cosine, sine, 0.0f, cosine},
        {-sine, cosine, 0.0f, -sine},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
    }};
    board = compose_affine(board, rotation);
    for (std::size_t lane = 0; lane < 4; ++lane) {
        board[3][lane] = rs::mul_add(board[1][lane], output.squish, board[3][lane]);
    }
}

}  // namespace s3::physics::skeleton_output::wobble
