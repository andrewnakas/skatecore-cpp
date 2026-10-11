// Port of skate-core/src/player/tests/{post_input,post_output,post_state,pre_state,state_phase}.rs.
#include <vector>

#include "harness.hpp"
#include "s3/core/player/post_input.hpp"
#include "s3/core/player/post_output.hpp"
#include "s3/core/player/post_state.hpp"
#include "s3/core/player/pre_state.hpp"
#include "s3/core/player/state_phase.hpp"

namespace post_input_tests {
using namespace s3::player::post_input;

struct Event {
    enum Kind { Grind, Trajectory, Scalar, Register } kind;
    CandidateRegistration registration = CandidateRegistration::First1888;
    bool operator==(const Event& o) const { return kind == o.kind && registration == o.registration; }
};
std::ostream& operator<<(std::ostream& os, const Event& e) {
    return os << "Event(" << int(e.kind) << ", " << int(e.registration) << ")";
}

struct Services : PostInputServices {
    std::vector<Event> events;
    std::uint8_t trajectory = 0;
    float scalar = 0.0f;
    void update_grind_manager_82d8ab08() override { events.push_back({Event::Grind}); }
    std::uint8_t update_trajectory_selector_82d68800() override {
        events.push_back({Event::Trajectory});
        return trajectory;
    }
    float calculate_scalar_2740_82db5e10() override {
        events.push_back({Event::Scalar});
        return scalar;
    }
    void register_candidate_82762ab0(CandidateRegistration r) override { events.push_back({Event::Register, r}); }
};

PostInputPlayerFields player() { return PostInputPlayerFields{{1, 2, 3, 4}, 0, 4, 8, 0}; }
PostInputProcessedFields processed() {
    PostInputProcessedFields p{};
    p.current_state_2508 = 100;
    return p;
}
PostInputPhysOutFields phys_out() { return PostInputPhysOutFields{false, false, {10, 11, 12, 13}, false}; }
CandidatePublicationFields candidates() {
    CandidatePublicationFields c{};
    c.staged_latched_12776 = 7;
    return c;
}

S3_TEST(phase_preserves_service_order_and_native_counter_publication) {
    auto pl = player();
    auto pr = processed();
    auto output = phys_out();
    output.reset_state_frames_316 = true;
    output.capture_jump_reference_442 = true;
    auto c = candidates();
    c.first_object_present_196 = true;
    c.first_pending_288 = true;
    c.second_object_present_500 = true;
    c.second_pending_592 = true;
    Services services;
    services.trajectory = 3;
    services.scalar = s3::rs::from_bits(0x3E800000u);
    run_post_input(PostInputContext{pl, pr, output, c}, services);
    const std::vector<Event> expected{{Event::Grind},
                                      {Event::Trajectory},
                                      {Event::Scalar},
                                      {Event::Register, CandidateRegistration::First1888},
                                      {Event::Register, CandidateRegistration::Second2176}};
    CHECK_EQ(services.events, expected);
    CHECK_EQ(pl.state_frames_1304, 1u);
    CHECK_EQ(pr.state_frames_2572, 1u);
    CHECK_EQ(pl.jump_fix_frames_1308, 1u);
    CHECK_EQ(pr.jump_fix_frames_2576, 1u);
    CHECK_EQ(pl.jump_reference_1264, output.jump_reference_128);
    CHECK_EQ(pr.jump_reference_848, output.jump_reference_128);
    CHECK_EQ(pr.flags_2468 & 0x400u, 0x400u);
    CHECK_EQ(s3::rs::to_bits(pr.scalar_2740), 0x3E800000u);
    CHECK_EQ(pr.flags_2480 & 0x00600000u, 0x00600000u);
    CHECK(!c.first_pending_288);
    CHECK(!c.second_pending_592);
    CHECK(output.complete_76);
}

S3_TEST(reset_branch_clears_state_latch_and_staged_candidate_follows_validity) {
    auto pl = player();
    pl.flags_1296 = 0x40000000u;
    auto pr = processed();
    pr.flags_2468 = 0x00400000u;
    auto output = phys_out();
    auto c = candidates();
    c.staged_word_12768 = 0x11223344u;
    c.staged_valid_12772 = 9;
    c.staged_pending_12780 = true;
    Services services;
    run_post_input(PostInputContext{pl, pr, output, c}, services);
    CHECK_EQ(pl.state_frames_1304, 0u);
    CHECK_EQ(pr.state_frames_2572, 0u);
    CHECK_EQ(pl.flags_1296 & 0x40000000u, 0u);
    CHECK_EQ(c.staged_latched_12776, 1u);
    CHECK_EQ(pr.flags_2480 & 0x00100000u, 0x00100000u);
    CHECK_EQ(pr.word_2464, 0x11223344u);
    CHECK(!c.staged_pending_12780);
}

S3_TEST(latch_helper_preserves_the_twenty_frame_strict_expiry_and_mirrors_bits) {
    auto pl = player();
    pl.flags_1296 = 0x06000000u;
    pl.latch_frames_1320 = 20;
    auto pr = processed();
    detail::update_flag_latches_82db5cf8(pl, pr);
    CHECK_EQ(pl.latch_frames_1320, 21u);
    CHECK_EQ(pl.flags_1296 & 0x06000000u, 0x06000000u);
    detail::update_flag_latches_82db5cf8(pl, pr);
    CHECK_EQ(pl.flags_1296 & 0x06000000u, 0u);
    CHECK_EQ(pr.flags_2472 & 0x0000000Cu, 0u);
    pr.flags_2472 = 0x00008010u;
    pr.flags_2480 = 0x00020000u;
    detail::update_flag_latches_82db5cf8(pl, pr);
    CHECK_EQ(pl.flags_1296 & 0x18020000u, 0x18020000u);
}
}  // namespace post_input_tests

namespace post_output_tests {
using namespace s3::player::post_output;
enum class Event { Player, Skeleton };
struct Services : PostOutputServices {
    std::vector<Event> events;
    void finish_player_output_82909510(PlayerOutputField1392&) override { events.push_back(Event::Player); }
    void finish_skeleton_output_82be2ba8() override { events.push_back(Event::Skeleton); }
};
S3_TEST(preserves_the_complete_native_call_order) {
    PlayerOutputField1392 field;
    Services services;
    run_post_output(field, services);
    CHECK((services.events == std::vector<Event>{Event::Player, Event::Skeleton}));
}
}  // namespace post_output_tests

namespace post_state_tests {
using namespace s3::player::post_state;
struct Services : PostStateServices {
    std::uint32_t calls = 0;
    void update_current_state_vtable_12() override { calls += 1; }
};
S3_TEST(dispatches_the_current_state_post_state_slot_once) {
    Services services;
    run_post_state(services);
    CHECK_EQ(services.calls, 1u);
}
}  // namespace post_state_tests

namespace pre_state_tests {
using namespace s3::player::pre_state;
enum class Event { Fill, Component, State };
struct Services : PreStateServices {
    std::vector<Event> events;
    std::array<std::uint32_t, 4> vector{};
    void fill_packet_vtable_24(PreStatePacket& packet) override {
        events.push_back(Event::Fill);
        std::copy(vector.begin(), vector.end(), packet.words.begin() + 12);
    }
    void update_component_1840_82d74270() override { events.push_back(Event::Component); }
    void update_before_state_vtable_4() override { events.push_back(Event::State); }
};
S3_TEST(publishes_packet_vector_and_preserves_full_native_call_order) {
    PreStatePlayerFields player{9, true};
    PreStateSkeletonFields skeleton{{0, 0, 0, 0}, false, true};
    const std::array<std::uint32_t, 4> vector{0x3F800000u, 0xC0000000u, 0x40400000u, 0};
    Services services;
    services.vector = vector;
    run_pre_state(player, skeleton, services);
    CHECK_EQ(player.frame_counter_1312, 10u);
    CHECK_EQ(skeleton.predicted_position_16112, vector);
    CHECK(skeleton.predicted_position_set_16416);
    CHECK(!skeleton.nested_flag_3184);
    CHECK((services.events == std::vector<Event>{Event::Fill, Event::Component, Event::State}));
}
S3_TEST(absent_component_skips_only_82d74270_and_counter_wraps) {
    PreStatePlayerFields player{UINT32_MAX, false};
    PreStateSkeletonFields skeleton{{7, 7, 7, 7}, false, true};
    Services services;
    services.vector = {1, 2, 3, 4};
    run_pre_state(player, skeleton, services);
    CHECK_EQ(player.frame_counter_1312, 0u);
    CHECK((services.events == std::vector<Event>{Event::Fill, Event::State}));
    CHECK_EQ(skeleton.predicted_position_16112, (std::array<std::uint32_t, 4>{1, 2, 3, 4}));
}
}  // namespace pre_state_tests

namespace state_phase_tests {
using namespace s3::player::state_phase;
struct Event {
    enum Kind { State, Controller, Mode, BoardTouch, Forces, FixedStep } kind;
    std::uint32_t mode = 0;
    bool operator==(const Event& o) const { return kind == o.kind && mode == o.mode; }
};
std::ostream& operator<<(std::ostream& os, const Event& e) { return os << "Event(" << int(e.kind) << ", " << e.mode << ")"; }
struct Services : StatePhaseServices {
    std::vector<Event> events;
    void update_current_state_vtable_8() override { events.push_back({Event::State}); }
    void update_controller_82d75f00() override { events.push_back({Event::Controller}); }
    void update_controller_mode_1_82d751d8() override { events.push_back({Event::Mode, 1}); }
    void update_controller_mode_2_82d750f0() override { events.push_back({Event::Mode, 2}); }
    void update_controller_mode_3_82d75bb8() override { events.push_back({Event::Mode, 3}); }
    void update_controller_mode_4_82d75d58() override { events.push_back({Event::Mode, 4}); }
    void touch_skateboard_vtable_116() override { events.push_back({Event::BoardTouch}); }
    void apply_skateboard_force_queue_82c03718() override { events.push_back({Event::Forces}); }
    void update_skateboard_fixed_step_cache_82db61f0() override { events.push_back({Event::FixedStep}); }
};
S3_TEST(active_controller_mode_runs_between_state_and_board_work) {
    for (std::uint32_t mode = 1; mode <= 4; ++mode) {
        StatePhaseFields fields{1.0f, s3::rs::from_bits(0x3C888889u), mode, true};
        Services services;
        run_state_phase(fields, services);
        const std::vector<Event> expected{{Event::State},      {Event::Controller}, {Event::Mode, mode},
                                          {Event::BoardTouch}, {Event::Forces},     {Event::FixedStep}};
        CHECK_EQ(services.events, expected);
        CHECK_EQ(s3::rs::to_bits(fields.elapsed_1344), s3::rs::to_bits(1.0f + s3::rs::from_bits(0x3C888889u)));
    }
}
S3_TEST(inactive_or_unknown_controller_skips_only_controller_calls) {
    const std::pair<bool, std::uint32_t> cases[] = {{false, 2}, {true, 0}, {true, 5}};
    for (const auto& [system_on, mode] : cases) {
        StatePhaseFields fields{-0.0f, 0.0f, mode, system_on};
        Services services;
        run_state_phase(fields, services);
        const std::vector<Event> expected =
            system_on ? std::vector<Event>{{Event::State}, {Event::Controller}, {Event::BoardTouch}, {Event::Forces},
                                           {Event::FixedStep}}
                      : std::vector<Event>{{Event::State}, {Event::BoardTouch}, {Event::Forces}, {Event::FixedStep}};
        CHECK_EQ(services.events, expected);
        CHECK_EQ(s3::rs::to_bits(fields.elapsed_1344), 0u);
    }
}
}  // namespace state_phase_tests
