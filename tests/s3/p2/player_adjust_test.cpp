// Port of skate-core/src/player/tests/adjust.rs.
#include <vector>

#include "harness.hpp"
#include "s3/core/player/adjust.hpp"

namespace {
using namespace s3::player::adjust;

struct Event {
    enum Kind { Condition, BoardFlag, Adjust, State, Selector, Finish } kind;
    bool flag = false;
    std::uint32_t value = 0;
    bool operator==(const Event& o) const { return kind == o.kind && flag == o.flag && value == o.value; }
};
std::ostream& operator<<(std::ostream& os, const Event& e) {
    return os << "Event(" << int(e.kind) << ", " << e.flag << ", " << e.value << ")";
}

struct Services : AdjustServices {
    std::uint32_t condition = 0;
    bool board_flag = false;
    std::uint32_t selector = 0;
    std::vector<Event> events;

    std::uint32_t board_condition_82c02138() override {
        events.push_back({Event::Condition});
        return condition;
    }
    bool controller_mode_one_board_flag_868() override {
        events.push_back({Event::BoardFlag});
        return board_flag;
    }
    void adjust_skeleton_82bd80d8(bool flag, float timestep) override {
        events.push_back({Event::Adjust, flag, s3::rs::to_bits(timestep)});
    }
    void update_current_state_post_physics_vtable_40() override { events.push_back({Event::State}); }
    std::uint32_t player_output_selector_82db9100() override {
        events.push_back({Event::Selector});
        return selector;
    }
    void finish_skeleton_adjustment_82bd83e0(std::uint32_t s) override {
        events.push_back({Event::Finish, false, s});
    }
};

S3_TEST(publishes_board_condition_and_preserves_post_solver_call_order) {
    AdjustFields fields{100, s3::rs::from_bits(0x3c888889u), 1, false, 7.0f, UINT32_MAX};
    Services services;
    services.condition = 1;
    services.board_flag = true;
    services.selector = 19;
    run_adjust(fields, services);
    CHECK(fields.output_flag_42);
    CHECK_EQ(s3::rs::to_bits(fields.output_scalar_144), 0u);
    CHECK_EQ(fields.output_counter_200, 0u);
    const std::vector<Event> expected{{Event::Condition}, {Event::BoardFlag}, {Event::Adjust, true, 0x3c888889u},
                                      {Event::State},     {Event::Selector},  {Event::Finish, false, 19}};
    CHECK_EQ(services.events, expected);
}

S3_TEST(state_500_and_other_controller_modes_skip_only_the_native_branches) {
    AdjustFields fields{500, 0.25f, 4, false, 7.0f, 8};
    Services services;
    services.condition = 1;
    services.board_flag = true;
    services.selector = 3;
    run_adjust(fields, services);
    CHECK(!fields.output_flag_42);
    CHECK_EQ(fields.output_scalar_144, 7.0f);
    CHECK_EQ(fields.output_counter_200, 8u);
    const std::vector<Event> expected{{Event::Condition},
                                      {Event::Adjust, false, s3::rs::to_bits(0.25f)},
                                      {Event::State},
                                      {Event::Selector},
                                      {Event::Finish, false, 3}};
    CHECK_EQ(services.events, expected);
}
}  // namespace
