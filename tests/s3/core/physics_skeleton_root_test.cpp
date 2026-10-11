// Port of the inline #[cfg(test)] module in skate-core/src/physics/skeleton_root.rs.
#include "harness.hpp"
#include "s3/core/physics/skeleton_root.hpp"

namespace {
using namespace s3::physics::skeleton_root;
using s3::physics::skeleton_animation_record::compose_affine;
using s3::physics::skeleton_animation_record::IDENTITY;

S3_TEST(actual_board_prediction_and_source_heading_build_the_animation_frame) {
    SkeletonRootFrames roots = SkeletonRootFrames::default_();
    AnimationPartTransform board = IDENTITY;
    board[3] = {10.0f, 2.0f, 0.0f, 0.0f};
    AnimationPartTransform animation = IDENTITY;
    animation[3] = {1.0f, 2.0f, 3.0f, 0.0f};
    roots.update(board, {5.0f, 0.0f, 0.0f, 0.0f}, 0.02f, animation, IDENTITY);
    CHECK(s3::rs::abs(roots.predicted_board_position[0] - 10.1f) < 1e-6f);
    const auto recentered = compose_affine(roots.animation_to_board, animation);
    for (float v : recentered[3]) CHECK(s3::rs::abs(v) < 1e-6f);
    CHECK(!roots.initialize_heading);
    roots.supplied_prediction = std::array<float, 4>{30.0f, 40.0f, 50.0f, 0.0f};
    roots.update(board, {5.0f, 0.0f, 0.0f, 0.0f}, 0.02f, animation, IDENTITY);
    CHECK_EQ(roots.predicted_board_position, (std::array<float, 4>{30.0f, 40.0f, 50.0f, 0.0f}));
    CHECK(!roots.supplied_prediction.has_value());
    CHECK_EQ(roots.previous_board_position, board[3]);
    const auto product = compose_affine(roots.world_to_animation, roots.animation_to_world);
    for (std::size_t axis = 0; axis < 4; ++axis) {
        for (std::size_t lane = 0; lane < 4; ++lane) {
            CHECK(s3::rs::abs(product[axis][lane] - IDENTITY[axis][lane]) < 1e-5f);
        }
    }
}
}  // namespace
