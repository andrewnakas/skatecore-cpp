// Port of skate-core/src/physics/tests/skeleton_board_offset.rs.
#include "harness.hpp"
#include "s3/core/physics/skeleton_board_offset.hpp"

namespace {
using namespace s3::physics::skeleton_board_offset;
using s3::physics::skeleton_animation_record::IDENTITY;

std::array<AnimationPartTransform, 4> identity_targets() {
    std::array<AnimationPartTransform, 4> t{};
    t.fill(IDENTITY);
    return t;
}

S3_TEST(refresh_applies_without_decay_then_height_and_xz_decay_independently) {
    SkateboardOffset offset = SkateboardOffset::default_();
    AnimationPartTransform transform = IDENTITY;
    transform[3] = {15.0f, 30.0f, 45.0f, 0.0f};
    offset.refresh_transform(transform);
    AnimationPartTransform board = IDENTITY;
    auto targets = identity_targets();
    offset.update(board, targets);
    CHECK_EQ(board[3], transform[3]);
    CHECK_EQ(targets[3][3], transform[3]);
    CHECK_EQ(offset.orientation_frames, 15.0f);
    offset.refresh_height(10.0f, 7.0f);
    board = IDENTITY;
    targets = identity_targets();
    offset.update(board, targets);
    const float ratio = 14.0f / 15.0f;
    CHECK_EQ(offset.orientation_frames, 14.0f);
    CHECK_EQ(offset.height_frames, 7.0f);
    CHECK_EQ(board[3][0], 15.0f * (ratio * ratio));
    CHECK_EQ(board[3][1], 10.0f);
    CHECK_EQ(board, targets[0]);
    offset.orientation_frames = 0.0f;
    offset.height_frames = 1.0f;
    board = IDENTITY;
    offset.update(board, targets);
    CHECK_EQ(offset.height_frames, 0.0f);
    CHECK_EQ(offset.transform[3][1], 0.0f);
    CHECK(!offset.height_refreshed);
}

/// A non-finite row must not survive the decay's Gram-Schmidt.
///
/// A zero-length row's `frsqrte` estimate is infinite, so the unguarded normalise produced NaN,
/// and a NaN already in the row stayed NaN. Either way it reached the board pose, then
/// `animation_to_world`, then the skeleton's lifted-COM target velocity, and the solver's
/// finiteness check aborted the game. This decay is armed for 15 updates after a landing, which
/// is where the abort was seen.
S3_TEST(a_non_finite_offset_row_decays_without_going_non_finite) {
    SkateboardOffset offset = SkateboardOffset::default_();
    AnimationPartTransform transform = IDENTITY;
    transform[0].fill(s3::rs::f32::NAN_);
    offset.refresh_transform(transform);
    for (int update = 0; update < 20; ++update) {
        AnimationPartTransform board = IDENTITY;
        auto targets = identity_targets();
        offset.update(board, targets);
        // The refresh update applies the frame as written and decays nothing, so the injected row
        // is still NaN there; from the first decaying update on, the row must come back finite.
        if (update == 0) {
            continue;
        }
        for (std::size_t axis = 0; axis < 4; ++axis) {
            for (float v : offset.transform[axis]) {
                CHECK_MSG(s3::rs::is_finite(v), "update " << update << ": offset row " << axis);
            }
        }
        for (const auto& row : board) {
            for (float v : row) CHECK_MSG(s3::rs::is_finite(v), "update " << update << ": board pose");
        }
    }
}
}  // namespace
