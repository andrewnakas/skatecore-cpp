// Port of skate-core/src/player/tests/lifecycle.rs.
#include <memory>
#include <vector>

#include "harness.hpp"
#include "s3/core/player/lifecycle.hpp"

namespace {
using namespace s3::player::lifecycle;
using s3::player::state::PhysicalStateId;

struct Event {
    enum Kind { GetType, Exit, Enter, Hold, LetGo } kind;
    StateBinding binding{};
    StateCall call{};
    bool operator==(const Event& o) const {
        return kind == o.kind && binding == o.binding && call == o.call;
    }
};
std::ostream& operator<<(std::ostream& os, const Event& e) {
    return os << "Event(" << int(e.kind) << ", " << unsigned(e.binding.state) << ", "
              << unsigned(e.call.state.state) << ")";
}
Event get_type(StateBinding b) { return Event{Event::GetType, b, {}}; }
Event exit_(StateCall c) { return Event{Event::Exit, {}, c}; }
Event enter(StateCall c) { return Event{Event::Enter, {}, c}; }
const Event HOLD{Event::Hold, {}, {}};
const Event LET_GO{Event::LetGo, {}, {}};

struct Recorder : PhysicalStateCalls, SkateboardControllerActions {
    PhysicalStateId reported_type;
    std::shared_ptr<std::vector<Event>> events_ = std::make_shared<std::vector<Event>>();
    explicit Recorder(PhysicalStateId t) : reported_type(t) {}
    std::vector<Event>& events() { return *events_; }
    PhysicalStateId get_type(StateBinding state) override {
        events_->push_back(::get_type(state));
        return reported_type;
    }
    void exit(StateCall call) override { events_->push_back(exit_(call)); }
    void enter(StateCall call) override { events_->push_back(::enter(call)); }
    void hold_skateboard() override { events_->push_back(HOLD); }
    void let_go_of_skateboard() override { events_->push_back(LET_GO); }
};

StateChangeData seeded_data() {
    StateChangeData d;
    d.player = PlayerStateChangeFields{0x13121312u, 400, 13.44f};
    d.processed = ProcessedStateChangeFields{0,           0x25002500u, 0x25042504u, 0x25082508u, 0x25122512u,
                                             0x25162516u, 0x25202520u, 0x25642564u, 26.64f};
    d.skateboard_controller = SkateboardControllerFields{444, 0, false};
    return d;
}
std::uint32_t id(PhysicalStateId s) { return static_cast<std::uint32_t>(s); }

S3_TEST(exits_old_pointer_before_switching_and_enters_new_pointer_afterward) {
    const auto old = StateBinding::new_(PhysicalStateId::PhysicsGround);
    const auto new_ = StateBinding::new_(PhysicalStateId::PhysicsAir);
    auto lifecycle = PhysicalPlayerStateLifecycle::new_(old.state);
    auto data = seeded_data();
    Recorder states(old.state);
    Recorder controller(old.state);
    CHECK(lifecycle.set_physics_state(id(new_.state), data, states, controller).unwrap() == new_);
    const std::vector<Event> expected{get_type(old), exit_(StateCall{old, old}), enter(StateCall{new_, new_})};
    CHECK_EQ(states.events(), expected);
    CHECK(lifecycle.active() == new_);
}

S3_TEST(publishes_exact_state_history_and_resets_transition_fields) {
    auto lifecycle = PhysicalPlayerStateLifecycle::new_(PhysicalStateId::PhysicsGround);
    auto data = seeded_data();
    Recorder states(PhysicalStateId::PhysicsGround);
    Recorder controller(PhysicalStateId::PhysicsGround);
    lifecycle.set_physics_state(id(PhysicalStateId::PhysicsAir), data, states, controller).unwrap();
    CHECK_EQ(data.player.word_1312, 0u);
    CHECK_EQ(s3::rs::to_bits(data.player.scalar_1344), s3::rs::to_bits(0.0f));
    CHECK_EQ(s3::rs::to_bits(data.processed.scalar_2664), s3::rs::to_bits(0.0f));
    CHECK_EQ(data.processed.word_2564, 0u);
    CHECK_EQ(data.processed.previous_state_2504, 100u);
    CHECK_EQ(data.processed.previous_category_2516, 100u);
    CHECK_EQ(data.processed.requested_state_2500, 200u);
    CHECK_EQ(data.processed.current_state_2508, 200u);
    CHECK_EQ(data.processed.current_category_2512, 200u);
    CHECK_EQ(data.player.previous_category_latch_1336, 100u);
    CHECK_EQ(data.processed.previous_category_latch_2520, 100u);

    states.reported_type = PhysicalStateId::PhysicsAir;
    data.player.previous_category_latch_1336 = 700;
    lifecycle.set_physics_state(id(PhysicalStateId::KnownAir), data, states, controller).unwrap();
    CHECK_EQ(data.processed.previous_state_2504, 200u);
    CHECK_EQ(data.processed.previous_category_2516, 200u);
    CHECK_EQ(data.processed.current_state_2508, 201u);
    CHECK_EQ(data.processed.current_category_2512, 200u);
    CHECK_EQ(data.player.previous_category_latch_1336, 700u);
    CHECK_EQ(data.processed.previous_category_latch_2520, 700u);
}

S3_TEST(every_native_state_selects_its_exact_owned_pointer_slot) {
    using P = PhysicalStateId;
    const std::pair<P, std::uint32_t> expected[] = {
        {P::PhysicsGround, 1696},   {P::SlideGround, 1700},     {P::RevertGround, 1704},
        {P::GroundAnimation, 1708}, {P::Skitching, 1772},       {P::FollowPath, 1760},
        {P::PhysicsAir, 1712},      {P::KnownAir, 1720},        {P::PhysicsAirSecondary, 1716},
        {P::WipeoutGround, 1748},   {P::GrindBoardslide, 1728}, {P::GrindFiftyFifty, 1732},
        {P::GrindTipslide, 1736},   {P::GrindFiveO, 1740},      {P::GrindBackslash, 1744},
        {P::GrindDarkslide, 1724},  {P::BipedGround, 1764},     {P::BipedAir, 1768},
        {P::OffBoardPushing, 1776}, {P::LandingOnDeck, 1792},   {P::HandPlant, 1780},
        {P::FootPlant, 1784},       {P::Boneless, 1788},        {P::Sleeping, 1692},
        {P::Nonspecific, 1752},     {P::Teleporting, 1756},
    };
    for (const auto& [requested, owner_offset] : expected) {
        auto lifecycle = PhysicalPlayerStateLifecycle::new_(P::Sleeping);
        auto data = seeded_data();
        Recorder states(P::Sleeping);
        Recorder controller(P::Sleeping);
        const auto selected = lifecycle.set_physics_state(id(requested), data, states, controller).unwrap();
        CHECK(selected == (StateBinding{requested, owner_offset}));
        CHECK(!states.events().empty());
        CHECK_EQ(states.events().back(), enter(StateCall{selected, selected}));
    }
}

S3_TEST(invalid_ids_fail_before_callbacks_writes_or_pointer_changes) {
    auto lifecycle = PhysicalPlayerStateLifecycle::new_(PhysicalStateId::PhysicsGround);
    const auto active_before = lifecycle.active();
    auto data = seeded_data();
    const auto data_before = data;
    Recorder states(PhysicalStateId::PhysicsGround);
    Recorder controller(PhysicalStateId::PhysicsGround);
    using R = s3::rs::Result<StateBinding, UnknownPhysicalState>;
    CHECK(lifecycle.set_physics_state(203, data, states, controller) ==
          R(s3::rs::make_err(UnknownPhysicalState{203})));
    CHECK(lifecycle.active() == active_before);
    CHECK(data == data_before);
    CHECK(states.events().empty());
    CHECK(controller.events().empty());
}

S3_TEST(offboard_entry_chooses_held_or_free_mode_from_native_flag_bits) {
    auto held_lifecycle = PhysicalPlayerStateLifecycle::new_(PhysicalStateId::PhysicsGround);
    auto held_data = seeded_data();
    held_data.skateboard_controller.state_448 = 4;
    Recorder held_states(PhysicalStateId::PhysicsGround);
    Recorder held_actions(PhysicalStateId::PhysicsGround);
    held_lifecycle.set_physics_state(id(PhysicalStateId::BipedGround), held_data, held_states, held_actions).unwrap();
    CHECK_EQ(held_actions.events(), std::vector<Event>{HOLD});
    CHECK_EQ(held_data.skateboard_controller.word_444, 0u);
    CHECK_EQ(held_data.skateboard_controller.state_448, 1u);
    CHECK(held_data.skateboard_controller.system_on_452);

    auto free_lifecycle = PhysicalPlayerStateLifecycle::new_(PhysicalStateId::PhysicsGround);
    auto free_data = seeded_data();
    free_data.processed.flags_2480 = 0x80;
    free_data.skateboard_controller.state_448 = 1;
    Recorder free_states(PhysicalStateId::PhysicsGround);
    Recorder free_actions(PhysicalStateId::PhysicsGround);
    free_lifecycle.set_physics_state(id(PhysicalStateId::OffBoardPushing), free_data, free_states, free_actions)
        .unwrap();
    CHECK_EQ(free_actions.events(), std::vector<Event>{LET_GO});
    CHECK_EQ(free_data.skateboard_controller.word_444, 0u);
    CHECK_EQ(free_data.skateboard_controller.state_448, 2u);
    CHECK(free_data.skateboard_controller.system_on_452);
}

S3_TEST(onboard_entry_runs_stop_controller_before_state_callbacks) {
    auto lifecycle = PhysicalPlayerStateLifecycle::new_(PhysicalStateId::BipedGround);
    auto data = seeded_data();
    data.skateboard_controller = SkateboardControllerFields{99, 1, true};
    Recorder states(PhysicalStateId::BipedGround);
    Recorder actions = states;  // shares the event log (Rc<RefCell<..>> clone)
    lifecycle.set_physics_state(id(PhysicalStateId::PhysicsGround), data, states, actions).unwrap();
    CHECK_EQ(data.skateboard_controller.word_444, 0u);
    CHECK_EQ(data.skateboard_controller.state_448, 0u);
    CHECK(!data.skateboard_controller.system_on_452);
    const auto old = StateBinding::new_(PhysicalStateId::BipedGround);
    const auto new_ = StateBinding::new_(PhysicalStateId::PhysicsGround);
    const std::vector<Event> expected{LET_GO, get_type(old), exit_(StateCall{old, old}),
                                      enter(StateCall{new_, new_})};
    CHECK_EQ(states.events(), expected);
}

S3_TEST(an_already_running_offboard_controller_is_left_untouched) {
    auto lifecycle = PhysicalPlayerStateLifecycle::new_(PhysicalStateId::BipedGround);
    auto data = seeded_data();
    data.processed.flags_2480 = 0x180;
    data.skateboard_controller = SkateboardControllerFields{99, 1, true};
    Recorder states(PhysicalStateId::BipedGround);
    Recorder actions(PhysicalStateId::BipedGround);
    lifecycle.set_physics_state(id(PhysicalStateId::BipedAir), data, states, actions).unwrap();
    CHECK(actions.events().empty());
    CHECK(data.skateboard_controller == (SkateboardControllerFields{99, 1, true}));
}
}  // namespace
