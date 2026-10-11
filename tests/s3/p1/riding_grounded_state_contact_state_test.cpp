// Port of the inline tests in skate-core/src/riding/grounded/state/contact_state.rs.
#include <string>
#include <vector>

#include "harness.hpp"
#include "s3/core/riding/grounded/state/contact_state.hpp"

namespace {
using namespace s3::riding::grounded::state::contact_state;
using s3::rs::Result;
using s3::rs::Unit;

struct Services {
    using Error = Unit;  // Rust `()`
    using Handle = std::uint32_t;

    std::vector<std::string> events;
    std::optional<std::uint32_t> initial;
    std::optional<std::uint32_t> identified;
    std::optional<std::optional<std::uint32_t>> published;

    Result<Unit, Error> stop_contact_tracking_82d62f20() {
        events.push_back("stop");
        return Unit{};
    }
    Result<Unit, Error> clear_inactive_contact_output_3024() {
        events.push_back("clear");
        return Unit{};
    }
    Result<std::optional<Handle>, Error> initial_contact_handle_1920() {
        events.push_back("initial");
        return initial;
    }
    Result<std::optional<Handle>, Error> identify_contact_82d63c30(const GroundContactStateInput&) {
        events.push_back("identify");
        return identified;
    }
    Result<Unit, Error> publish_contact_82d61268(const GroundContactStateInput&, const Handle* retained) {
        events.push_back("publish");
        published = retained != nullptr ? std::optional<std::uint32_t>(*retained) : std::nullopt;
        return Unit{};
    }
};

GroundContactStateInput input() {
    GroundContactStateInput i;
    i.flags_2476 = 0x00400000u;
    i.flags_2480 = 0x10000000u;
    i.velocity_400 = {1.0f, 1.0f, 1.0f, 1.0f};
    i.axis_464 = {0.0f, 0.5f, 0.0f, 0.0f};
    i.contact_position_560 = {2.0f, 2.0f, 2.0f, 2.0f};
    i.trajectory_position_592 = {3.0f, 3.0f, 3.0f, 3.0f};
    i.reckoning_vector_1200 = {4.0f, 4.0f, 4.0f, 4.0f};
    i.differing_contact_frame_limit = 5;
    return i;
}

using Strings = std::vector<std::string>;

S3_TEST(inactive_branch_preserves_history_and_runs_both_owner_calls) {
    GroundContactHistory<std::uint32_t> history{7, 9u};
    auto frame = input();
    frame.flags_2476 = 0;
    Services services{};

    update(history, frame, services).unwrap();

    CHECK_EQ(services.events, (Strings{"stop", "clear"}));
    CHECK_EQ(history.differing_contact_frames_2756, 7);
    CHECK(history.tracked_contact_2760 == std::optional<std::uint32_t>(9u));
}

S3_TEST(candidate_latches_initial_handle_then_counts_identity_changes) {
    auto history = GroundContactHistory<std::uint32_t>::empty();
    Services services{};
    services.initial = 10u;
    services.identified = 11u;

    update(history, input(), services).unwrap();

    CHECK_EQ(services.events, (Strings{"initial", "identify", "publish"}));
    CHECK_EQ(history.differing_contact_frames_2756, 1);
    CHECK(history.tracked_contact_2760 == std::optional<std::uint32_t>(10u));
    CHECK(services.published == std::optional<std::optional<std::uint32_t>>(std::optional<std::uint32_t>(10u)));
}

S3_TEST(limit_and_axis_rejection_clear_only_the_native_fields) {
    GroundContactHistory<std::uint32_t> history{5, 10u};
    auto frame = input();
    frame.differing_contact_frame_limit = 5;
    Services services{};
    services.identified = 11u;
    update(history, frame, services).unwrap();
    CHECK_EQ(history.differing_contact_frames_2756, 6);
    CHECK(!history.tracked_contact_2760.has_value());
    CHECK(services.published == std::optional<std::optional<std::uint32_t>>(std::optional<std::uint32_t>()));

    history.differing_contact_frames_2756 = 13;
    history.tracked_contact_2760 = 12u;
    frame.axis_464[1] = 0.95f;
    services.events.clear();
    services.published = std::nullopt;
    update(history, frame, services).unwrap();
    CHECK_EQ(history.differing_contact_frames_2756, 13);
    CHECK(!history.tracked_contact_2760.has_value());
    CHECK_EQ(services.events, (Strings{"publish"}));
}

}  // namespace
