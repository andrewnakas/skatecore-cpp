// Port of the inline tests in skate-core/src/riding/grounded/state/update.rs.
#include <string>
#include <tuple>
#include <vector>

#include "harness.hpp"
#include "s3/core/riding/grounded/state/update.hpp"

namespace {
using namespace s3::riding::grounded::state::update;
using s3::point_graph::PointGraph;
using s3::riding::grounded::state::contact_state::GroundContactStateInput;
using s3::rs::Result;
using s3::rs::Unit;

using Str = const char*;  // Rust `&'static str`

struct Services {
    // PumpingGeometry / GroundContactServices / GroundUpdateServices share one error type.
    using Error = Str;
    using Handle = std::uint32_t;
    using UpdateError = Str;
    using ContactHandle = std::uint32_t;

    std::vector<std::string> events;
    bool collision_signal = false;

    // PumpingGeometry
    Result<float, Error> height(std::array<float, 4>, std::array<float, 4>) {
        events.push_back("pumping-height");
        return 2.0f;
    }
    Result<float, Error> speed(std::array<float, 4>, std::array<float, 4>, float) {
        return s3::rs::make_err(Str("unexpected speed"));
    }
    Result<float, Error> angular_speed(std::array<float, 4>, std::array<float, 4>, const PumpingSample&, float) {
        return s3::rs::make_err(Str("unexpected angular speed"));
    }
    Result<float, Error> inclination_radians(float) { return s3::rs::make_err(Str("unexpected inclination")); }

    // GroundContactServices
    Result<Unit, Error> stop_contact_tracking_82d62f20() {
        events.push_back("contact-stop");
        return Unit{};
    }
    Result<Unit, Error> clear_inactive_contact_output_3024() {
        events.push_back("contact-clear");
        return Unit{};
    }
    Result<std::optional<Handle>, Error> initial_contact_handle_1920() {
        events.push_back("contact-initial");
        return std::optional<Handle>();
    }
    Result<std::optional<Handle>, Error> identify_contact_82d63c30(const GroundContactStateInput&) {
        events.push_back("contact-identify");
        return std::optional<Handle>();
    }
    Result<Unit, Error> publish_contact_82d61268(const GroundContactStateInput&, const Handle*) {
        events.push_back("contact-publish");
        return Unit{};
    }

    // GroundUpdateServices
    Result<Unit, UpdateError> set_elapsed_skeleton_flag() {
        events.push_back("elapsed-flag");
        return Unit{};
    }
    Result<std::tuple<PumpingSettings, PumpingMode, PumpingSample>, UpdateError> pumping_frame() {
        events.push_back("pumping-frame");
        const PointGraph<8> graph{};
        PumpingSettings settings{graph, graph, graph, graph, graph, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        PumpingMode mode{0.0f, 0.0f, 0.0f, 0.0f};
        PumpingSample sample;
        sample.position = {0.0f, 0.0f, 0.0f, 0.0f};
        sample.normal = {0.0f, 1.0f, 0.0f, 0.0f};
        sample.com_to_deck_world = {0.0f, 0.0f, 0.0f, 0.0f};
        sample.deck_angle = 0.0f;
        sample.intentional_pumping = 0;
        return std::make_tuple(settings, mode, sample);
    }
    Result<Unit, UpdateError> update_skateboard(PhysicsGroundState& state) {
        events.push_back("skateboard");
        state.flag_2722 = collision_signal;
        return Unit{};
    }
    Result<Unit, UpdateError> predict_future_deck() {
        events.push_back("predict");
        return Unit{};
    }
    Result<Unit, UpdateError> set_collision_skeleton_flag() {
        events.push_back("collision-flag");
        return Unit{};
    }
    Result<Unit, UpdateError> set_ground_skeleton_flag() {
        events.push_back("ground-flag");
        return Unit{};
    }
    Result<Unit, UpdateError> update_special_state() {
        events.push_back("special");
        return Unit{};
    }
    Result<Unit, UpdateError> update_trajectory() {
        events.push_back("trajectory");
        return Unit{};
    }
};

PhysicsGroundState state() {
    PhysicsGroundState s{};
    s.anti_flip_torque_2624 = {9.0f, 9.0f, 9.0f, 9.0f};
    s.steering_push_scalar_2640 = 1.0f;
    s.elapsed_2648 = 0.1f;
    s.scalar_2664 = 8.0f;
    s.straighten_scale_2672 = 1.0f;
    s.anti_flip_nudge_applied_2723 = true;
    s.pinning_2727 = true;
    s.flag_2729 = true;
    return s;
}

GroundUpdateInput input(std::uint32_t flags_2476) {
    GroundUpdateInput i;
    i.wheel_contact_count_2556 = 1;
    i.position_112 = {3.0f, 4.0f, 5.0f, 0.0f};
    i.timestep_2604 = 0.25f;
    i.contact_state = GroundContactStateInput{};
    i.contact_state.flags_2476 = flags_2476;
    return i;
}

using Strings = std::vector<std::string>;

S3_TEST(update_preserves_complete_outer_order_and_state_prefix) {
    auto s = state();
    auto pumping = PumpingState::reset_state();
    auto contacts = GroundContactHistory<std::uint32_t>::empty();
    Services services{};
    services.collision_signal = true;

    update(s, pumping, contacts, input(0), services).unwrap();

    CHECK_EQ(services.events, (Strings{
                                  "elapsed-flag",
                                  "contact-stop",
                                  "contact-clear",
                                  "pumping-frame",
                                  "pumping-height",
                                  "skateboard",
                                  "predict",
                                  "collision-flag",
                                  "ground-flag",
                                  "trajectory",
                              }));
    CHECK_EQ(s.captured_position_x_2656, 3.0f);
    CHECK_EQ(s.captured_position_z_2660, 5.0f);
    CHECK(s.captured_position_valid_2726);
    CHECK_EQ(s.anti_flip_torque_2624, (std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}));
    CHECK(!s.anti_flip_nudge_applied_2723);
    CHECK(!s.pinning_2727);
    CHECK(!s.flag_2729);
    CHECK_EQ(s.scalar_2664, -1.0f);
    CHECK_EQ(s.elapsed_2648, 0.35f);
}

S3_TEST(active_contact_and_special_tail_select_only_their_native_paths) {
    auto s = state();
    s.elapsed_2648 = 0.0f;
    auto pumping = PumpingState::reset_state();
    auto contacts = GroundContactHistory<std::uint32_t>::empty();
    Services services{};

    update(s, pumping, contacts, input(0x00400000u), services).unwrap();

    CHECK_EQ(services.events, (Strings{
                                  "contact-publish",
                                  "pumping-frame",
                                  "pumping-height",
                                  "skateboard",
                                  "predict",
                                  "ground-flag",
                                  "special",
                              }));
}

}  // namespace
