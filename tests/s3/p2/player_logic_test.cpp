// Port of skate-core/src/player/tests/logic.rs.
#include <memory>
#include <vector>

#include "harness.hpp"
#include "s3/core/player/logic.hpp"

namespace {
using namespace s3::player::logic;
using namespace s3::player::lifecycle;
using s3::player::selector::ProcessedStateInput;
using s3::player::selector::conditions::BoardBodyState;
using s3::player::selector::conditions::SkeletonAnimationState;
using s3::player::selector::conditions::TwoStageThresholds;
using State = s3::player::state::PhysicalStateId;

struct Event {
    enum Kind { GetType, Exit, Enter } kind;
    State state;
    bool operator==(const Event& o) const { return kind == o.kind && state == o.state; }
};
std::ostream& operator<<(std::ostream& os, const Event& e) {
    return os << "Event(" << int(e.kind) << ", " << unsigned(e.state) << ")";
}

struct States : PhysicalStateCalls {
    std::shared_ptr<std::vector<Event>> events;
    State get_type(StateBinding state) override {
        events->push_back({Event::GetType, state.state});
        return state.state;
    }
    void exit(StateCall call) override { events->push_back({Event::Exit, call.state.state}); }
    void enter(StateCall call) override { events->push_back({Event::Enter, call.state.state}); }
};
struct Controller : SkateboardControllerActions {
    void hold_skateboard() override {}
    void let_go_of_skateboard() override {}
};

StateSelectionInput input() {
    const TwoStageThresholds thresholds{1.0f, 1.0f, 1.0f};
    StateSelectionInput in;
    ProcessedStateInput p{};
    p.grind_type_1248 = 6;
    p.category_2512 = 100;
    in.processed = p;
    in.skateboard_contact_count_869 = 0;
    in.board_body = BoardBodyState{0.0f, 0.0f};
    in.skeleton = SkeletonAnimationState{1, 0.0f, 1.0f};
    in.skitching_off_ground = thresholds;
    in.normal_off_ground = thresholds;
    return in;
}
StateChangeData change_data() {
    StateChangeData d;
    d.player = PlayerStateChangeFields{1, 0, 1.0f};
    d.processed = ProcessedStateChangeFields{0, 0, 0, 100, 100, 0, 0, 1, 1.0f};
    d.skateboard_controller = SkateboardControllerFields{0, 0, false};
    return d;
}
PhysicsOutputStaging staging() {
    PhysicsOutputStaging s;
    s.bytes_20_through_53.fill(0xaa);
    s.scalars_56_through_188.fill(1.0f);
    s.word_200 = 7;
    return s;
}

S3_TEST(unchanged_state_calls_get_type_once_and_still_clears_output_staging) {
    auto events = std::make_shared<std::vector<Event>>();
    auto logic = PhysicalPlayerLogic::new_(State::PhysicsGround);
    auto data = change_data();
    auto output = staging();
    States states;
    states.events = events;
    Controller controller;
    const auto result = logic.run(input(), data, output, states, controller).unwrap();
    CHECK(!result.changed);
    CHECK_EQ(*events, (std::vector<Event>{{Event::GetType, State::PhysicsGround}}));
    CHECK_EQ(logic.frames_since_jump_1304, 0u);
    CHECK_EQ(output.bytes_20_through_53, (std::array<std::uint8_t, 34>{}));
    CHECK_EQ(output.scalars_56_through_188, (std::array<float, 34>{}));
    CHECK_EQ(output.word_200, 0u);
}

S3_TEST(state_edge_runs_native_transition_and_sets_100) {
    auto events = std::make_shared<std::vector<Event>>();
    auto logic = PhysicalPlayerLogic::new_(State::PhysicsGround);
    auto frame = input();
    frame.processed.flags_2480 = 0x1000;
    auto data = change_data();
    auto output = staging();
    States states;
    states.events = events;
    Controller controller;
    const auto result = logic.run(frame, data, output, states, controller).unwrap();
    CHECK(result.changed);
    CHECK(result.selected.state == State::LandingOnDeck);
    CHECK_EQ(logic.frames_since_jump_1304, 100u);
    const std::vector<Event> expected{{Event::GetType, State::PhysicsGround},
                                      {Event::GetType, State::PhysicsGround},
                                      {Event::Exit, State::PhysicsGround},
                                      {Event::Enter, State::LandingOnDeck}};
    CHECK_EQ(*events, expected);
    CHECK_EQ(data.processed.current_state_2508, 503u);
}
}  // namespace
