// Port of skate-core/src/player/selector/tests.rs and selector/tests/offboard.rs.
#include "harness.hpp"
#include "s3/core/player/selector.hpp"

namespace {
using namespace s3::player::selector;
using conditions::BoardBodyState;
using conditions::SkeletonAnimationState;
using conditions::TwoStageThresholds;
using State = s3::player::state::PhysicalStateId;

constexpr TwoStageThresholds thresholds() { return TwoStageThresholds{1.0f, 1.0f, 1.0f}; }

StateSelectionInput input(std::uint32_t category) {
    StateSelectionInput in{};
    in.processed.grind_type_1248 = 6;
    in.processed.grind_candidate_1488 = false;
    in.processed.category_2512 = category;
    in.skateboard_contact_count_869 = 0;
    in.board_body = BoardBodyState{0.0f, 0.0f};
    in.skeleton = SkeletonAnimationState{1, 0.0f, 1.0f};
    in.skitching_off_ground = thresholds();
    in.normal_off_ground = thresholds();
    return in;
}

State calculate(StateSelector& selector, State state, const StateSelectionInput& in) {
    return selector.calculate(state, in);
}

S3_TEST(teleport_request_has_priority_and_fallthrough_sets_the_native_request_byte) {
    StateSelector selector{};
    auto frame = input(100);
    frame.processed.flags_2472 = 0x40000;
    CHECK_EQ(calculate(selector, State::PhysicsGround, frame), State::Teleporting);

    frame.processed.flags_2472 = 0;
    frame.processed.category_2512 = 200;
    selector.air_frames = 300;
    CHECK_EQ(calculate(selector, State::PhysicsAir, frame), State::PhysicsAir);
    CHECK(selector.request_teleport);
}

S3_TEST(all_six_grind_types_use_the_exact_native_state_mapping) {
    const State expected[] = {
        State::GrindFiftyFifty, State::GrindBoardslide, State::GrindTipslide,
        State::GrindFiveO,      State::GrindBackslash,  State::GrindDarkslide,
    };
    for (std::uint32_t grind_type = 0; grind_type < 6; ++grind_type) {
        StateSelector selector{};
        auto frame = input(100);
        frame.processed.grind_candidate_1488 = true;
        frame.processed.grind_type_1248 = grind_type;
        CHECK_EQ(calculate(selector, State::PhysicsGround, frame), expected[grind_type]);
    }
}

S3_TEST(nonspecific_uses_exact_two_three_and_collision_counter_thresholds) {
    StateSelector free_{};
    {
        const auto frame = input(700);
        CHECK_EQ(calculate(free_, State::Nonspecific, frame), State::Nonspecific);
        CHECK_EQ(calculate(free_, State::Nonspecific, frame), State::Nonspecific);
        CHECK_EQ(calculate(free_, State::Nonspecific, frame), State::PhysicsAir);
    }

    StateSelector two_wheels{};
    {
        auto frame = input(700);
        frame.skateboard_contact_count_869 = 2;
        for (int i = 0; i < 2; ++i) {
            CHECK_EQ(calculate(two_wheels, State::Nonspecific, frame), State::Nonspecific);
        }
        CHECK_EQ(calculate(two_wheels, State::Nonspecific, frame), State::PhysicsGround);
    }

    StateSelector collision{};
    {
        auto frame = input(700);
        frame.processed.flags_2468 = 0x10000;
        for (int i = 0; i < 60; ++i) {
            CHECK_EQ(calculate(collision, State::Nonspecific, frame), State::Nonspecific);
        }
        CHECK_EQ(calculate(collision, State::Nonspecific, frame), State::PhysicsGround);
    }
}

S3_TEST(nonspecific_short_collision_threshold_comes_from_flags_2484_bit_21) {
    StateSelector selector{};
    auto frame = input(700);
    frame.processed.flags_2468 = 0x10000;
    frame.processed.flags_2484 = 0x200000;
    for (int i = 0; i < 30; ++i) {
        CHECK_EQ(calculate(selector, State::Nonspecific, frame), State::Nonspecific);
    }
    CHECK_EQ(calculate(selector, State::Nonspecific, frame), State::PhysicsGround);
}

S3_TEST(ground_to_landing_on_deck_takes_the_native_noop_call_branch) {
    StateSelector selector{};
    auto frame = input(100);
    frame.processed.flags_2480 = 0x1000;
    const State state = selector.calculate(State::PhysicsGround, frame);
    CHECK_EQ(state, State::LandingOnDeck);
}

S3_TEST(revert_normal_exit_sets_the_recovered_state_changer_byte) {
    StateSelector selector{};
    const auto frame = input(100);
    CHECK_EQ(calculate(selector, State::RevertGround, frame), State::PhysicsGround);
    CHECK(selector.revert_exited_normally);
}

S3_TEST(known_air_transition_order_matches_the_tu3_tree) {
    StateSelector selector{};
    auto frame = input(200);
    frame.processed.flags_2480 = 0x2000000;
    CHECK_EQ(calculate(selector, State::KnownAir, frame), State::FootPlant);

    frame.processed.flags_2480 = 0;
    frame.processed.flags_2476 = 0x80;
    frame.processed.trajectory_collision_time_2772 = 0.04f;
    CHECK_EQ(calculate(selector, State::KnownAir, frame), State::WipeoutGround);
}

S3_TEST(physics_air_collision_resolves_before_the_grind_candidate_path) {
    StateSelector selector{};
    selector.post_grind_jump_counter = 11;
    auto frame = input(200);
    frame.processed.state_timer_2664 = 0.1f;
    frame.processed.flags_2468 = 0x10000;
    frame.processed.grind_candidate_1488 = true;
    frame.processed.grind_type_1248 = 0;
    CHECK_EQ(calculate(selector, State::PhysicsAir, frame), State::PhysicsGround);
}

S3_TEST(skitch_exit_countdown_is_loaded_to_ten_then_saturates) {
    StateSelector selector{};
    auto frame = input(100);
    frame.processed.flags_2476 = 0x200000;
    CHECK_EQ(calculate(selector, State::Skitching, frame), State::Skitching);
    CHECK_EQ(selector.skitch_exit_countdown, 10);

    frame.processed.flags_2476 = 0;
    (void)calculate(selector, State::PhysicsGround, frame);
    CHECK_EQ(selector.skitch_exit_countdown, 9);
}

S3_TEST(check_for_grind_rejects_both_native_flags) {
    for (std::uint32_t rejected : {0x20000u, 0x4000000u}) {
        StateSelector selector{};
        auto frame = input(100);
        frame.processed.grind_candidate_1488 = true;
        frame.processed.grind_type_1248 = 0;
        frame.processed.flags_2480 = rejected;
        CHECK_EQ(calculate(selector, State::PhysicsGround, frame), State::PhysicsGround);
    }
}

S3_TEST(skeleton_animation_predicate_uses_mode_and_sign_cleared_lane) {
    using conditions::is_skateboard_animated;
    CHECK(is_skateboard_animated(SkeletonAnimationState{2, 0.0f, 100.0f}));
    CHECK(is_skateboard_animated(SkeletonAnimationState{0, -2.0f, 1.0f}));
    CHECK(!is_skateboard_animated(SkeletonAnimationState{1, -2.0f, 1.0f}));
    CHECK(is_skateboard_animated(SkeletonAnimationState{0, s3::rs::f32::NAN_, 1.0f}));
}

// ---- selector/tests/offboard.rs ----
StateSelectionInput offboard_input(std::uint32_t flags_2484, std::uint32_t flags_2476, std::uint32_t flags_2480) {
    const TwoStageThresholds t{1.0f, 1.0f, 1.0f};
    StateSelectionInput in{};
    in.processed.grind_type_1248 = 0;
    in.processed.grind_candidate_1488 = false;
    in.processed.flags_2476 = flags_2476;
    in.processed.flags_2480 = flags_2480;
    in.processed.flags_2484 = flags_2484;
    in.processed.category_2512 = 500;
    in.processed.trajectory_collision_time_2772 = 1.0f;
    in.skateboard_contact_count_869 = 0;
    in.board_body = BoardBodyState{0.0f, 0.0f};
    in.skeleton = SkeletonAnimationState{0, 0.0f, 1.0f};
    in.normal_off_ground = t;
    in.skitching_off_ground = t;
    return in;
}

S3_TEST(biped_air_uses_published_air_flag_not_board_on_ground_animation_flag) {
    for (std::uint32_t animation_board_on_ground : {0u, 1u}) {
        StateSelector selector{};
        const auto airborne = offboard_input(0x10000u | animation_board_on_ground, 0x80, 0);
        CHECK_EQ(selector.calculate(State::BipedAir, airborne), State::BipedAir);
        const auto landed = offboard_input(animation_board_on_ground, 0x80, 0);
        CHECK_EQ(selector.calculate(State::BipedAir, landed), State::BipedGround);
    }
}

S3_TEST(air_to_ground_does_not_oscillate_while_published_air_flag_remains_set) {
    StateSelector selector{};
    const auto airborne = offboard_input(0x10000, 0x8000, 0);
    State state = State::BipedGround;
    for (int i = 0; i < 60; ++i) {
        state = selector.calculate(state, airborne);
        CHECK_EQ(state, State::BipedAir);
    }
    state = selector.calculate(state, offboard_input(0, 0x8000, 0));
    CHECK_EQ(state, State::BipedGround);
}

S3_TEST(actual_mount_and_land_on_board_requests_keep_their_native_precedence) {
    StateSelector selector{};
    CHECK_EQ(selector.calculate(State::BipedAir, offboard_input(0x10000, 0, 0)), State::PhysicsAir);
    CHECK_EQ(selector.calculate(State::BipedAir, offboard_input(0x10000, 0x80, 0x10)), State::LandingOnDeck);
}

S3_TEST(airborne_dismount_reads_high_byte_gate_in_both_onboard_air_states) {
    for (State current : {State::PhysicsAir, State::KnownAir}) {
        StateSelector selector{};
        for (std::uint32_t board_on_ground : {0u, 1u}) {
            const auto allowed = offboard_input(board_on_ground, 0x80, 0);
            CHECK_EQ(selector.calculate(current, allowed), State::BipedAir);
            const auto blocked = offboard_input(0x01000000u | board_on_ground, 0x80, 0);
            CHECK_NE(selector.calculate(current, blocked), State::BipedAir);
        }
    }
}

}  // namespace
