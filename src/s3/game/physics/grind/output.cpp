// Port of skate-game/src/physics/grind/output.rs.
#include "s3/game/physics/grind/output.hpp"

namespace s3::game::physics::grind::output {

SideEffects fill(Family family, const State& state, const ManagerObservation& manager, PhysicalOutput& output) {
    const auto geometry = manager.geometry;
    output.crouch = state.crouch;
    output.leaving = state.leaving;
    output.direction = state.direction;
    output.normal = state.normal;
    output.across = state.across;
    output.point = geometry.point_1120;
    output.primitive_endpoints = {geometry.primitive_start_1264, geometry.primitive_end_1280};
    output.impact_speed = geometry.impact_speed_1492;
    output.audio_surface = manager.surface.audio_surface_1468;
    output.substate = state.substate;
    output.is_ledge = geometry.kind_1464 == 2;
    output.curb = (geometry.flags_1476 & 0x40000000u) != 0;
    output.classification = state.classification_104;
    if (geometry.spline_guids_1296) {
        output.spline_guids = *geometry.spline_guids_1296;
    }
    if (output.is_ledge) {
        output.high_side = geometry.high_side_1440;
    }
    output.family = family;
    if (family == Family::Tipslide || family == Family::FiveO || family == Family::Backslash) {
        output.on_front = (manager.control.flags_1516 & 0x20000000u) != 0;
    }
    if (family == Family::Tipslide) {
        output.tipslide_324_325_326 = state.tipslide_97_98_99;
    }
    SideEffects effects;
    if (state.just_jumped) effects.air_jump_velocity_128 = state.jump_velocity;
    if (state.slide_wipeout && (family == Family::Boardslide || family == Family::Darkslide)) {
        effects.wipeout_impulse_16 = state.slide_impulse;
    }
    return effects;
}

/// Adapt the retained native publication directly. Unwritten family fields,
/// camera state, graph names and selector flags are not reset by FillOut.
SideEffects fill_fields(Family family, const State& state, const ManagerObservation& manager,
                        GrindOutputFields& out) {
    auto v = [](std::array<std::uint32_t, 4> x) { return rs::from_bits(x); };
    PhysicalOutput physical{
        v(out.direction_0),
        v(out.point_16),
        v(out.normal_32),
        v(out.across_48),
        {v(out.primitive_start_64), v(out.primitive_end_80)},
        v(out.high_side_112),
        out.impact_speed_128,
        out.crouch_132,
        family,
        out.words_136_140[1],
        out.audio_surface_216,
        out.spline_guids_224_232,
        out.trick_out_240,
        out.leaving_317 != 0,
        out.flag_318 != 0,
        out.is_ledge_320 != 0,
        out.curb_321 != 0,
        {out.dropping_in_324 != 0, out.tipslide_325 != 0, out.tipslide_326 != 0},
    };
    const SideEffects side_effects = fill(family, state, manager, physical);
    auto raw = [](V x) { return rs::to_bits(x); };
    out.direction_0 = raw(physical.direction);
    out.point_16 = raw(physical.point);
    out.normal_32 = raw(physical.normal);
    out.across_48 = raw(physical.across);
    out.primitive_start_64 = raw(physical.primitive_endpoints[0]);
    out.primitive_end_80 = raw(physical.primitive_endpoints[1]);
    // The optional stores retain the complete existing bytes, not boolized data.
    if (manager.geometry.kind_1464 == 2) {
        out.high_side_112 = raw(physical.high_side);
    }
    if (manager.geometry.spline_guids_1296) {
        out.spline_guids_224_232 = *manager.geometry.spline_guids_1296;
    }
    out.impact_speed_128 = physical.impact_speed;
    out.crouch_132 = physical.crouch;
    out.words_136_140 = {static_cast<std::uint32_t>(family), physical.substate};
    out.audio_surface_216 = physical.audio_surface;
    out.trick_out_240 = physical.classification;
    out.leaving_317 = physical.leaving ? 1 : 0;
    out.is_ledge_320 = physical.is_ledge ? 1 : 0;
    out.curb_321 = physical.curb ? 1 : 0;
    if (family == Family::Tipslide || family == Family::FiveO || family == Family::Backslash) {
        out.flag_318 = physical.on_front ? 1 : 0;
    }
    if (family == Family::Tipslide) {
        out.dropping_in_324 = physical.tipslide_324_325_326[0] ? 1 : 0;
        out.tipslide_325 = physical.tipslide_324_325_326[1] ? 1 : 0;
        out.tipslide_326 = physical.tipslide_324_325_326[2] ? 1 : 0;
    }
    return side_effects;
}

}  // namespace s3::game::physics::grind::output
