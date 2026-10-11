// Port of skate-game/src/physics/offboard/skeleton_ground/frames_tests.rs
// (mounted as physics::offboard::skeleton_ground::frames::tests).
#include <cmath>

#include "harness.hpp"
#include "s3/core/animation/foot_ik/state.hpp"
#include "s3/core/animation/foot_ik/status.hpp"
#include "s3/game/physics/offboard/skeleton_ground.hpp"

using namespace s3::game::physics::offboard::skeleton_ground::frames;
using s3::game::physics::offboard::skeleton_ground::Input;
using s3::game::physics::offboard::skeleton_ground::Transform;
using s3::physics::skeleton_animation_record::IDENTITY;

S3_TEST(biped_ik_bindings_keep_adjacent_foot_volumes_distinct) {
    using s3::animation::foot_ik::state::LIMBS;
    //TU3 Init82BED74C..770:1120 target parts and1136 adjacent parts.
    //These are physical part indices, not animation hierarchy indices.
    const std::array<std::size_t, 4> parts{15, 19, 3, 7};
    const std::array<std::optional<std::size_t>, 4> parents{16, 20, std::nullopt, std::nullopt};
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK_EQ(LIMBS[i].part, parts[i]);
        CHECK(LIMBS[i].parent_part == parents[i]);
    }
}

S3_TEST(disabling_feet_target_ik_fades_board_weight_without_disabling_the_pose) {
    using namespace s3::animation::foot_ik::status;
    std::array<LimbStatus, 4> limbs{};
    for (std::size_t i = 0; i < 2; ++i) {
        limbs[i].mode = Mode::OnDeck;
        limbs[i].board_blend = 0.5f;
    }
    //Synthetic exact-binary test step, not a replacement stock setting.
    BlendSettings settings{};
    settings.hand_inner_padding = {0.0f, 0.0f, 0.0f, 0.0f};
    settings.hand_outer_padding = {1.0f, 1.0f, 1.0f, 1.0f};
    settings.external_blend_step = 0.125f;
    settings.board_blend_step = 0.25f;
    //82BEEE58..88 selects target zero from2480bit27 and bounds its delta.
    update_blends(limbs, 1.0f, 1.0f, 0x08000000, settings);
    CHECK_EQ(limbs[0].board_blend, 0.25f);
    CHECK_EQ(limbs[1].board_blend, 0.25f);
    update_blends(limbs, 1.0f, 1.0f, 0x08000000, settings);
    update_blends(limbs, 1.0f, 1.0f, 0x08000000, settings);
    CHECK_EQ(limbs[0].board_blend, 0.0f);
    CHECK_EQ(limbs[1].board_blend, 0.0f);
    CHECK(limbs[0].mode == Mode::OnDeck && limbs[1].mode == Mode::OnDeck);
    CHECK(limbs[2].mode == Mode::Disabled && limbs[3].mode == Mode::Disabled);
    update_blends(limbs, 1.0f, 1.0f, 0, settings);
    CHECK_EQ(limbs[0].board_blend, 0.25f);
    CHECK_EQ(limbs[1].board_blend, 0.25f);
}

S3_TEST(frozen_board_uses_retained_pose_and_flips_complete_axes) {
    SkeletonRootFrames roots{};
    Transform retained = IDENTITY;
    retained[3] = {2.0f, 3.0f, 4.0f, 0.0f};
    Transform mapped = IDENTITY;
    mapped[3] = {9.0f, 9.0f, 9.0f, 9.0f};
    Transform world = IDENTITY;
    world[0][3] = 7.0f;
    world[2][3] = 8.0f;
    std::uint32_t flags = 2;
    const auto output = prepare(roots, world, mapped, retained, 4, 1, flags);
    CHECK((retained[3] == std::array<float, 4>{2.0f, 3.0f, 4.0f, 0.0f}));
    CHECK_EQ(roots.animation_to_world[0][3], -7.0f);
    CHECK_EQ(roots.animation_to_world[2][3], -8.0f);
    CHECK_EQ(output[3][0], -2.0f);
    CHECK_EQ(output[3][1], 3.0f);
    CHECK_EQ(output[3][2], -4.0f);
    CHECK_EQ(flags, 0x80002u);
    CHECK(roots.initialize_heading);
}

S3_TEST(biped_com_and_skate_root_use_old_basis_without_overwriting_physical_board) {
    SkeletonRootFrames roots{};
    const Transform old{{{0.0f, 0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f},
                         {10.0f, 20.0f, 30.0f, 0.0f}}};
    roots.reset_initial_alignment(old);
    SkeletonBoardFrames frames{};
    frames.physical_board[3] = {90.0f, 80.0f, 70.0f, 0.0f};
    const auto physical = frames.physical_board;
    Transform animation = IDENTITY;
    animation[3] = {2.0f, 3.0f, 4.0f, 0.0f};
    Transform mapped = IDENTITY;
    mapped[3] = {5.0f, 6.0f, 7.0f, 0.0f};
    Transform new_world = IDENTITY;
    new_world[3] = {100.0f, 200.0f, 300.0f, 0.0f};
    const std::array<float, 4> com{11.0f, 22.0f, 33.0f, 0.0f};
    Transform retained = IDENTITY;
    std::uint32_t flags = 0;
    auto target = prepare_pose(roots, frames, animation, mapped, retained, Input{&new_world, com}, 0, 0, flags);
    CHECK((frames.skate_root[3] == std::array<float, 4>{14.0f, 23.0f, 28.0f, 0.0f}));
    CHECK(frames.com_frame[3] == com);
    for (std::size_t axis = 0; axis < 3; ++axis) {
        for (std::size_t lane = 0; lane < 3; ++lane) {
            CHECK(std::fabs(frames.com_frame[axis][lane] - old[axis][lane]) < 1e-5f);
        }
    }
    CHECK(roots.animation_to_world == new_world);
    CHECK(retained == mapped);
    CHECK((target[3] == std::array<float, 4>{105.0f, 206.0f, 307.0f, 0.0f}));
    CHECK(frames.physical_board == physical);

    //Holding2484bit0 preserves LOCAL16016 while a new root moves the target.
    new_world[3][0] = 110.0f;
    target = prepare_pose(roots, frames, animation, IDENTITY, retained, Input{&new_world, com}, 0, 1, flags);
    CHECK(retained == mapped);
    CHECK((target[3] == std::array<float, 4>{115.0f, 206.0f, 307.0f, 0.0f}));
    CHECK(frames.physical_board == physical);
}
