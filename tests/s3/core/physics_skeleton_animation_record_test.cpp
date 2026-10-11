// Port of skate-core/src/physics/tests/skeleton_animation_record.rs.
#include "harness.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"

namespace {
using namespace s3::physics::skeleton_animation_record;
using A4 = std::array<float, 4>;

S3_TEST(com_uses_original_bone_volume_excludes_board_and_preserves_reset_order) {
    std::array<Vector3, 24> sizes{};
    sizes.fill(Vector3{0.0f, 0.0f, 0.0f});
    sizes[0] = Vector3{100.0f, 100.0f, 100.0f};
    sizes[1] = Vector3{1.0f, 1.0f, 1.0f};
    sizes[2] = Vector3{1.0f, 1.0f, 3.0f};
    std::array<std::uint32_t, 24> shapes{};
    shapes.fill(1);
    const auto masses = SkeletonAnimationMasses::from_bone_data(sizes, shapes, false);
    CHECK_EQ(masses.total, 4.0f);
    CHECK_EQ((std::array<float, 3>{masses.fractional[0], masses.fractional[1], masses.fractional[2]}),
             (std::array<float, 3>{0.0f, 0.25f, 0.75f}));
    std::array<AnimationPartTransform, 24> pose{};
    pose.fill(IDENTITY);
    pose[0][3] = {1.0f, 0.0f, 0.0f, 0.0f};
    pose[1][3] = {1.0f, 2.0f, 0.0f, 0.0f};
    pose[2][3] = {1.0f, 6.0f, 0.0f, 0.0f};
    AnimationPartTransform to_board = IDENTITY;
    to_board[3] = {100.0f, 20.0f, -10.0f, 0.0f};
    SkeletonAnimationRecord record = SkeletonAnimationRecord::default_();
    record.update(pose, to_board, masses);
    CHECK_EQ(record.centre_of_mass, (A4{1.0f, 5.0f, 0.0f, 0.0f}));
    CHECK_EQ(record.com_to_deck_world, (A4{0.0f, 5.0f, 0.0f, 0.0f}));
    CHECK_EQ(record.com_to_deck_world_delta, (A4{0.0f, 0.0f, 0.0f, 0.0f}));
    pose[1][3][1] += 4.0f;
    record.update(pose, to_board, masses);
    CHECK_EQ(record.com_to_deck_world_delta, (A4{0.0f, 1.0f, 0.0f, 0.0f}));
    record.reset_history();
    CHECK_EQ(record.reset_scalar, 1.0f);
    CHECK_EQ(record.pose, pose);
}

S3_TEST(global_bone_then_physics_frame_preserves_native_mapping_and_lanes) {
    const auto local = physics_bone_frame({0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 2.0f, 3.0f, 7.0f});
    CHECK_EQ(local[0], (A4{1.0f, 0.0f, 0.0f, 1.0f}));
    CHECK_EQ(local[1], (A4{0.0f, 1.0f, 0.0f, 0.0f}));
    CHECK_EQ(local[2], (A4{0.0f, 0.0f, 1.0f, 0.0f}));
    // A global quarter turn around Z makes multiplication order observable.
    const AnimationPartTransform global{{
        {0.0f, 1.0f, 0.0f, 11.0f},
        {-1.0f, 0.0f, 0.0f, 13.0f},
        {0.0f, 0.0f, 1.0f, 17.0f},
        {10.0f, 20.0f, 30.0f, 19.0f},
    }};
    std::array<std::size_t, 24> indices{};
    indices[1] = 1;
    std::array<AnimationPartTransform, 24> frames{};
    frames.fill(IDENTITY);
    frames[1] = local;
    const std::vector<AnimationPartTransform> bones{IDENTITY, global};
    const auto mapped = map_animation_parts(bones, indices, frames).unwrap();
    CHECK_EQ(mapped[0], IDENTITY);
    CHECK_EQ(mapped[1][0], global[0]);
    CHECK_EQ(mapped[1][1], global[1]);
    CHECK_EQ(mapped[1][2], global[2]);
    CHECK_EQ(mapped[1][3], (A4{8.0f, 21.0f, 33.0f, 107.0f}));
    indices[23] = 2;
    CHECK(map_animation_parts(bones, indices, frames).is_err());
}
}  // namespace
