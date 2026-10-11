// Port of skate-game/src/physics/skeleton_feedback.rs `mod tests`.
#include "harness.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/physics/skeleton_feedback.hpp"

using namespace s3::game::physics::skeleton_feedback;
using s3::physics::skeleton_animation_record::IDENTITY;
using s3::physics::skeleton_board_frames::SkeletonBoardFrames;
using s3::physics::skeleton_root::SkeletonRootFrames;
using A4 = std::array<float, 4>;

S3_TEST(retained_com_is_localized_before_new_physical_com_publication) {
    SkeletonRootFrames roots{};
    roots.world_to_animation[3] = {-10.0f, -20.0f, -30.0f, 0.0f};
    auto deck = IDENTITY;
    deck[3] = {15.0f, 26.0f, 37.0f, 0.0f};
    const A4 old_com{11.0f, 22.0f, 33.0f, 4.0f};
    const A4 new_com{13.0f, 26.0f, 39.0f, 8.0f};
    for (std::uint32_t flags : {0u, 0x400u}) {
        SkeletonBoardFrames frames{};
        frames.centre_of_mass = old_com;
        publish_board_observations(frames, roots, deck, new_com, 0.5f, flags);
        CHECK(frames.local_centre_of_mass == (A4{1.0f, 2.0f, 3.0f, 0.0f}));
        CHECK(frames.local_board_position == (A4{5.0f, 6.0f, 7.0f, 0.0f}));
        CHECK(frames.centre_of_mass == new_com);
        CHECK(frames.com_velocity == (A4{4.0f, 8.0f, 12.0f, 8.0f}));
        CHECK(frames.previous_centre_of_mass == (flags == 0 ? old_com : new_com));

        // The next observation uses the last published COM, but the new
        // root and deck. The history flag must not erase this distinction.
        roots.world_to_animation[3] = {-12.0f, -24.0f, -36.0f, 0.0f};
        deck[3] = {20.0f, 30.0f, 40.0f, 0.0f};
        publish_board_observations(frames, roots, deck, old_com, 0.5f, flags);
        CHECK(frames.local_centre_of_mass == (A4{1.0f, 2.0f, 3.0f, 0.0f}));
        CHECK(frames.local_board_position == (A4{8.0f, 6.0f, 4.0f, 0.0f}));
        CHECK(frames.centre_of_mass == old_com);
        CHECK(frames.com_velocity == (A4{-4.0f, -8.0f, -12.0f, -8.0f}));
        roots.world_to_animation[3] = {-10.0f, -20.0f, -30.0f, 0.0f};
        deck[3] = {15.0f, 26.0f, 37.0f, 0.0f};
    }
}
