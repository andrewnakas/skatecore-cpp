// Port of skate-core/src/player/offboard/toggle_board/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/toggle_board.hpp"

namespace {
using namespace s3::player::offboard::toggle_board;
using OC = std::optional<Command>;

Channel live(float remaining, float elapsed) { return Channel{true, remaining, elapsed}; }
Input retrieve() {
    Input i;
    i.retrieve_requested = true;
    i.retrieval_active = true;
    return i;
}
bool is_stop(const OC& c) { return c && std::holds_alternative<CommandStop>(c->value); }

S3_TEST(retrieval_runs_into_cycle_and_out_with_pulse_only_on_cycle_entry) {
    State state{};
    auto input = retrieve();
    const auto start = state.update(input, Channel{});
    CHECK(start.channel == OC(Command::BlendTo(Clip::FrontInto)));
    CHECK(start.retrieving && !start.retrieve);
    CHECK(state.update(input, live(0.251f, 0.0f)).channel == OC());
    const auto cycle = state.update(input, live(0.25f, 0.0f));
    CHECK(cycle.channel == OC(Command::SequenceTo(Clip::FrontCycle)));
    CHECK(cycle.retrieve);
    CHECK(!state.update(input, live(0.4f, 0.0f)).retrieve);
    CHECK(state.update(input, live(0.25f, 0.0f)).retrieve);
    input.holding_board = true;
    const auto out = state.update(input, live(0.4f, 0.0f));
    CHECK(out.channel == OC(Command::BlendTo(Clip::FrontOut)));
    CHECK(!out.retrieve);
    CHECK(state.update(input, live(0.4f, 0.16666667f)).channel == OC());
    const auto end = state.update(input, live(0.4f, 0.17f));
    CHECK(end.channel == OC(Command::Stop(0.16666667f)));
    CHECK(end.retrieving && end.yaw_pitch.has_value());
    CHECK(state.phase == Phase::Idle);
    input.retrieve_requested = false;
    CHECK(state.update(input, Channel{}) == Output{});
}

S3_TEST(drop_has_precedence_over_throw_and_playout_boundary_is_strict) {
    State state{};
    Input input;
    input.holding_board = true;
    input.drop_requested = true;
    input.throw_requested = true;
    CHECK(state.update(input, Channel{}).channel == OC(Command::BlendTo(Clip::Drop)));
    CHECK(state.update(input, live(1.0f, 0.5f)).channel == OC());
    CHECK(is_stop(state.update(input, live(1.0f, 0.5001f)).channel));
    input.drop_requested = false;
    CHECK(state.update(input, Channel{}).channel == OC(Command::BlendTo(Clip::Throw)));
}

S3_TEST(physical_guards_prevent_new_requests_but_do_not_cancel_running_drop) {
    State state{};
    auto a = retrieve();
    a.retrieval_blocked = true;
    CHECK(state.update(a, Channel{}) == Output{});
    auto b = retrieve();
    b.grabbing_object = true;
    CHECK(state.update(b, Channel{}) == Output{});
    auto c = retrieve();
    c.holding_board = true;
    CHECK(state.update(c, Channel{}) == Output{});
    Input drop;
    drop.holding_board = true;
    drop.drop_requested = true;
    state.update(drop, Channel{});
    auto grabbing = drop;
    grabbing.grabbing_object = true;
    const auto out = state.update(grabbing, live(0.6f, 0.1f));
    CHECK(out.dropping);
    CHECK(state.phase == Phase::DropPlaying);
}

S3_TEST(lost_retrieval_feedback_stops_before_possession_or_channel_checks) {
    State state{};
    state.update(retrieve(), Channel{});
    state.update(retrieve(), live(0.25f, 0.0f));
    auto in = retrieve();
    in.retrieval_active = false;
    in.holding_board = true;
    const auto out = state.update(in, Channel{});
    CHECK(is_stop(out.channel));
    CHECK(!out.retrieve);
}

S3_TEST(expired_into_channel_returns_idle_without_sending_stop) {
    for (const Channel& channel : {Channel{}, live(0.0f, 1.0f)}) {
        State state{};
        state.update(retrieve(), Channel{});
        const auto out = state.update(retrieve(), channel);
        CHECK(state.phase == Phase::Idle);
        CHECK(out.channel == OC());
        CHECK(out.retrieving);
    }
}

S3_TEST(yaw_uses_back_selection_and_five_degree_per_update_limit) {
    State state{};
    auto input = retrieve();
    input.yaw_radians = -2.0f;
    input.pitch_radians = 0.5f;
    const auto out = state.update(input, Channel{});
    CHECK(out.channel == OC(Command::BlendTo(Clip::BackInto)));
    const auto first = *out.yaw_pitch;
    CHECK_EQ(first, (std::array<float, 2>{2.0f * 57.295776f, 0.5f * 57.295776f}));
    auto zero = input;
    zero.yaw_radians = 0.0f;
    const auto next = *state.update(zero, live(1.0f, 0.0f)).yaw_pitch;
    CHECK_EQ(next[0], first[0] - 5.0f);
    state.begin();
    const auto reset = state.update(zero, Channel{});
    CHECK(reset.channel == OC(Command::BlendTo(Clip::FrontInto)));
    CHECK_EQ((*reset.yaw_pitch)[0], 0.0f);
}

S3_TEST(mirrored_yaw_applies_native_asymmetric_rear_sector_rule) {
    auto input = retrieve();
    input.yaw_radians = 2.0f;
    State normal{};
    auto out = normal.update(input, Channel{});
    CHECK_EQ((*out.yaw_pitch)[0], -90.0f);
    CHECK(out.channel == OC(Command::BlendTo(Clip::FrontInto)));
    State mirrored{};
    auto m = input;
    m.mirrored = true;
    out = mirrored.update(m, Channel{});
    CHECK_EQ((*out.yaw_pitch)[0], 2.0f * 57.295776f);
    CHECK(out.channel == OC(Command::BlendTo(Clip::BackInto)));
    State rear{};
    auto r = input;
    r.yaw_radians = 3.0f;
    CHECK_EQ((*rear.update(r, Channel{}).yaw_pitch)[0], 180.0f);
}
}  // namespace
