// Port of skate-game/src/physics/footplant.rs.
//! FootPlantManager state, original TU3 inline ctor82DB217C and Reset82D6F440.
//! Query results are retained separately from pending query ownership.
#include "s3/game/physics/footplant.hpp"

namespace s3::game::physics::footplant {

namespace {
Trajectory empty_trajectory() {
    Trajectory t;
    t.position = {0.0f, 0.0f, 0.0f, 0.0f};
    t.velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    t.acceleration = {0.0f, 0.0f, 0.0f, 0.0f};
    t.duration = -1.0f;
    return t;
}
}  // namespace

rs::Result<Footplant, std::string> Footplant::load(const sd::Collections& data) {
    //Deterministic host storage for fields the native ctor leaves unwritten.
    //The first-ever320 value is not claimed as native memory parity.
    Footplant state;
    state.enabled = false;
    state.result = QueryResult::miss();
    S3G_TRY(state.settings, Settings::load(data));
    state.selected_toe = std::nullopt;
    state.candidate = false;
    state.hit = false;
    state.perform = false;
    state.flag_627 = false;
    state.launch_valid = false;
    state.lock_valid = false;
    state.contact_active = false;
    state.requested = false;
    state.contact_time = -1.0f;
    state.scalar_596 = -1.0f;
    state.scalar_600 = 0.0f;
    state.target_blend = 0.0f;
    state.active_elapsed = 0.0f;
    state.scalar_612 = 0.0f;
    state.surface = 0;
    state.contact = {0.0f, 0.0f, 0.0f, 0.0f};
    state.adjusted_contact = {0.0f, 0.0f, 0.0f, 0.0f};
    state.current_up = {0.0f, 0.0f, 0.0f, 0.0f};
    state.selected_world = {0.0f, 0.0f, 0.0f, 0.0f};
    state.selected_record = {0.0f, 0.0f, 0.0f, 0.0f};
    state.leg_direction = {0.0f, 0.0f, 0.0f, 0.0f};
    state.vectors_352_368 = {};
    state.curve = {};
    state.physical_com = {0.0f, 0.0f, 0.0f, 0.0f};
    state.animation_com = {0.0f, 0.0f, 0.0f, 0.0f};
    state.launch_direction = {0.0f, 0.0f, 0.0f, 0.0f};
    state.locked_target = {0.0f, 0.0f, 0.0f, 0.0f};
    state.request = empty_trajectory();
    state.completed_trajectory = empty_trajectory();
    state.reset();
    return state;
}

void Footplant::reset() {
    contact_time = -1.0f;
    scalar_596 = -1.0f;
    selected_toe = std::nullopt;
    scalar_600 = 0.0f;
    target_blend = 0.0f;
    active_elapsed = 0.0f;
    scalar_612 = 0.0f;
    candidate = false;
    hit = false;
    perform = false;
    flag_627 = false;
    launch_valid = false;
    lock_valid = false;
    contact_active = false;
    requested = false;
    request = empty_trajectory();
    contact = {0.0f, 0.0f, 0.0f, 0.0f};
    adjusted_contact = {0.0f, 0.0f, 0.0f, 0.0f};
    vectors_352_368 = {};
    animation_com = {0.0f, 0.0f, 0.0f, 0.0f};
    locked_target = {0.0f, 0.0f, 0.0f, 0.0f};
    launch_direction = {0.0f, 0.0f, 0.0f, 0.0f};
    physical_com = {0.0f, 0.0f, 0.0f, 0.0f};
}

void Footplant::full_reset() {
    enabled = false;
    reset();
}

void Footplant::publish(sc::player::input_phase::AirOutputFields& out) const {
    if (hit) {
        out.footplant_contact_time_208 = contact_time;
        out.footplant_duration_212 = scalar_596;
        out.flag_447 = std::uint8_t(perform);
        out.flag_448 = std::uint8_t(perform || flag_627);
        out.footplant_surface_224 = surface;
        out.footplant_left_449 = std::uint8_t(flag_627 && selected_toe == std::optional<std::size_t>(15));
        out.footplant_right_450 = std::uint8_t(flag_627 && selected_toe == std::optional<std::size_t>(19));
        out.footplant_surface_height_216 = contact[1];
    }
}

}  // namespace s3::game::physics::footplant
