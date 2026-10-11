// Port of skate-core/src/animation/posture/tests.rs.
#include <string>
#include <vector>

#include "harness.hpp"
#include "s3/core/animation/posture.hpp"

using namespace s3::animation::posture;
using s3::rs::Result;
using s3::rs::Unit;

S3_TEST(native_default_stays_disabled_even_after_request) {
    PendingPosture state{};
    CHECK_EQ(state.profile(), 0u);
    CHECK(!state.is_pending());
    state.set_requested(true);
    auto tree = state.apply<int, Unit>(7, [](int, PosturePose) -> Result<int, Unit> {
        S3_PANIC("disabled profile wrapped");
    });
    CHECK(tree == (Result<int, Unit>(7)));
    CHECK(state.is_pending());
}

S3_TEST(profile_values_follow_registration_not_alphabetical_order) {
    const std::pair<std::uint32_t, std::string> cases[] = {
        {1, "POSTURE_STIFF_POSE"}, {2, "POSTURE_SLOUCH_POSE"}, {3, "POSTURE_BUFF_POSE"}};
    for (const auto& [value, name] : cases) {
        CHECK_EQ(std::string(posture_pose_name(s3::rs::unwrap(posture_pose_from_profile(value)))), name);
    }
    for (std::uint32_t value : {0u, 4u, 0xFFFFFFFFu}) {
        CHECK(posture_pose_from_profile(value) == std::nullopt);
    }
}

S3_TEST(failed_construction_keeps_request_for_fallback) {
    PendingPosture state{};
    state.set_profile(2);
    state.set_requested(true);
    using R0 = Result<Unit, std::string>;
    const R0 failed = state.apply<Unit, std::string>(Unit{}, [](Unit, PosturePose) -> R0 {
        return s3::rs::make_err(std::string("missing pose"));
    });
    CHECK(failed == R0(s3::rs::make_err(std::string("missing pose"))));
    CHECK(state.is_pending());
    using V = std::vector<PosturePose>;
    const V tree = state
                       .apply<V, Unit>(V{},
                                       [](V tree, PosturePose pose) -> Result<V, Unit> {
                                           tree.push_back(pose);
                                           return tree;
                                       })
                       .unwrap();
    CHECK(tree == (V{PosturePose::Slouch}));
    CHECK(!state.is_pending());
}

S3_TEST(synchronous_begins_each_wrap_without_changing_previous_tree) {
    PendingPosture state{};
    state.set_profile(1);
    using O = std::optional<PosturePose>;
    std::vector<O> trees;
    for (bool requested : {true, true, false}) {
        state.set_requested(requested);
        trees.push_back(
            state.apply<O, Unit>(std::nullopt, [](O, PosturePose pose) -> Result<O, Unit> { return O(pose); })
                .unwrap());
    }
    CHECK(trees == (std::vector<O>{PosturePose::Stiff, PosturePose::Stiff, std::nullopt}));
    CHECK(!state.is_pending());
}

S3_TEST(false_overwrites_unconsumed_request_and_refresh_does_not_request) {
    PendingPosture state{};
    state.set_requested(true);
    state.set_requested(false);
    state.set_profile(3);
    CHECK(state.selected_pose() == std::nullopt);
    state.set_requested(true);
    CHECK(state.selected_pose() == std::optional<PosturePose>(PosturePose::Buff));
    state.set_profile(99);
    CHECK(state.selected_pose() == std::nullopt);
    CHECK(state.is_pending());
    state.set_profile(1);
    CHECK(state.selected_pose() == std::optional<PosturePose>(PosturePose::Stiff));
}
