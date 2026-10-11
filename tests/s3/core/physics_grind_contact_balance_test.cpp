// Port of skate-core/src/physics/grind_contact/balance_tests.rs.
#include <string>

#include "harness.hpp"
#include "s3/core/physics/grind_contact/balance.hpp"

namespace {
using namespace s3::physics::grind_contact;
using namespace s3::physics::grind_contact::balance;
using s3::point_graph::PointGraph;

const V X{1.0f, 0.0f, 0.0f, 0.0f};
const V Y{0.0f, 1.0f, 0.0f, 0.0f};
const V Z{0.0f, 0.0f, 1.0f, 0.0f};

// Explicit synthetic geometry tests branches, not a substitute runtime map.
GrindSurface surface(GeometryType kind) {
    GrindSurface s;
    s.center = {0.0f, 0.0f, 0.0f, 0.0f};
    s.far_points = {negate(Z), Z};
    s.upmost_normal = Y;
    s.direction = Z;
    s.high_side = X;
    s.normal_limits = {0.0f, 6.0f};
    s.kind = kind;
    s.audio_surface = 0;
    s.physics_surface = 0;
    s.flags = 0;
    s.tilted_upmost_normal = Y;
    return s;
}
BalanceContact contact(const GrindSurface& surface, std::uint32_t kind, EntryKind entry_kind) {
    return BalanceContact{&surface, Z, Z, kind, entry_kind};
}
BalanceVectors vectors() { return BalanceVectors{Y, Y}; }
ExitLeanInput exit_input(float dt) { return ExitLeanInput{100, 100, 0, dt}; }
PointGraph<8> graph() {
    return PointGraph<8>{{0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f},
                         {0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f}};
}
void close(V a, V b) {
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK_MSG(s3::rs::abs(a[i] - b[i]) < 0.00001f, "lane" << i << ": " << a[i] << " != " << b[i]);
    }
}

S3_TEST(constructor_and_invalid_target_preserve_the_source_history) {
    BalanceState state = BalanceState::default_();
    CHECK(state == (BalanceState{0.0f, 0.0f, 2.0f, 21, Y, X}));
    const BalanceState original = state;
    BalanceVectors output{X, Z};
    state.update_target_up(TargetUpInput{400, 402, Y}, nullptr, output);
    CHECK(state == original);
    CHECK(output == (BalanceVectors{X, Z}));
}

S3_TEST(target_selects_previous_tip_state_and_separate_tilted_normal) {
    GrindSurface geometry = surface(GeometryType::Ledge);
    geometry.tilted_upmost_normal = X;
    const BalanceContact candidate = contact(geometry, 1, EntryKind::StayInGrind);
    for (std::uint32_t previous_state : {400u, 402u, 404u}) {
        BalanceState state = BalanceState::default_();
        BalanceVectors output = vectors();
        state.update_target_up(TargetUpInput{400, previous_state, Y}, &candidate, output);
        if (previous_state == 400) {
            const V raw{s3::rs::from_bits(0x3d999998u), 0.925f, 0.0f, 0.0f};
            const V expected = scale(raw, 1.0f / s3::rs::sqrt(raw[0] * raw[0] + raw[1] * raw[1]));
            close(output.grind_normal, expected);
        } else {
            close(output.grind_normal, Y);
        }
        CHECK_EQ(state.previous_normal, output.grind_normal);
    }
}

S3_TEST(target_non_grind_uses_current_up_and_projection_fallback) {
    const GrindSurface geometry = surface(GeometryType::ThinRail);
    const BalanceContact candidate = contact(geometry, 0, EntryKind::AirToGrind);
    BalanceState state = BalanceState::default_();
    BalanceVectors output = vectors();
    state.update_target_up(TargetUpInput{200, 402, X}, &candidate, output);
    close(output.grind_normal, X);
    state.update_target_up(TargetUpInput{200, 402, Z}, &candidate, output);
    close(output.grind_normal, Y);  // projected vector is exactly zero
}

S3_TEST(target_clamps_to_fourteen_degrees_with_the_native_rotation) {
    const GrindSurface geometry = surface(GeometryType::Ledge);
    const BalanceContact candidate = contact(geometry, 1, EntryKind::StayInGrind);
    BalanceState state = BalanceState::default_();
    BalanceVectors output = vectors();
    state.update_target_up(TargetUpInput{400, 400, X}, &candidate, output);
    const float angle = s3::rs::to_radians(14.0f);
    close(output.target_up, {s3::rs::sin(angle), s3::rs::cos(angle), 0.0f, 0.0f});
    close(state.previous_normal, Y);
    // Anti-parallel target: its zero cross axis selects grind_normal.
    state.update_target_up(TargetUpInput{400, 400, negate(Y)}, &candidate, output);
    close(output.target_up, Y);
}

S3_TEST(quaternion_rotation_preserves_axis_component_and_direction) {
    const float half_pi = s3::rs::f32::consts::FRAC_PI_2;
    close(rotate(Z, X, half_pi), Y);
    close(rotate(Z, Y, half_pi), negate(X));
    close(rotate(Y, Z, half_pi), X);
    close(rotate(Z, {1.0f, 0.0f, 2.0f, 0.0f}, half_pi), {0.0f, 1.0f, 2.0f, 0.0f});
    close(rotate(Z, X, -half_pi), negate(Y));
}

S3_TEST(exit_entry_delays_and_tipslide_geometry_eligibility) {
    const std::pair<EntryKind, float> cases[] = {
        {EntryKind::RideFromBelow, 0.8f}, {EntryKind::RideIntoCoping, 0.1f}, {EntryKind::RideFromAbove, 2.0f},
        {EntryKind::StayInGrind, 2.0f},   {EntryKind::ChangeGrind, 2.0f},    {EntryKind::AirToGrind, 2.0f},
        {EntryKind::DropIn, 2.0f},
    };
    for (const auto& [entry, delay] : cases) {
        const GrindSurface geometry = surface(GeometryType::Ledge);
        BalanceState state = BalanceState::default_();
        const BalanceContact c = contact(geometry, 1, entry);
        BalanceVectors v = vectors();
        state.update_exit_lean(exit_input(0.01f), &c, graph(), v);
        CHECK_EQ(state.entry_delay, delay);
        CHECK_EQ(state.elapsed, 0.01f);
        CHECK_EQ(state.frames_away, 0);
        CHECK_EQ(state.exit_angle_degrees, 0.0f);
    }
    for (GeometryType geometry_type : {GeometryType::ThinRail, GeometryType::FatRail, GeometryType::Ledge}) {
        for (std::uint32_t kind : {0u, 1u, 2u, 3u, 4u, 5u}) {
            const GrindSurface geometry = surface(geometry_type);
            BalanceState state = BalanceState::default_();
            const BalanceContact c = contact(geometry, kind, EntryKind::RideFromAbove);
            BalanceVectors v = vectors();
            state.update_exit_lean(exit_input(0.01f), &c, graph(), v);
            const bool starts =
                geometry_type == GeometryType::Ledge || (kind == 2 && geometry_type != GeometryType::ThinRail);
            CHECK_MSG((state.elapsed > 0.0f) == starts, "kind" << kind << " " << int(geometry_type));
        }
    }
}

S3_TEST(exit_substate_two_freezes_time_and_zero_dt_does_not_update_side) {
    GrindSurface geometry = surface(GeometryType::Ledge);
    geometry.high_side = negate(X);
    const BalanceContact candidate = contact(geometry, 1, EntryKind::RideIntoCoping);
    BalanceState state = BalanceState::default_();
    ExitLeanInput input = exit_input(0.2f);
    input.grind_substate = 2;
    BalanceVectors v = vectors();
    state.update_exit_lean(input, &candidate, graph(), v);
    CHECK_EQ(state.elapsed, 0.0f);
    CHECK_EQ(state.entry_delay, 0.1f);
    CHECK_EQ(state.exit_direction, X);
    input.grind_substate = 0;
    v = vectors();
    state.update_exit_lean(input, &candidate, graph(), v);
    CHECK_EQ(state.exit_direction, negate(X));
    const float old_time = state.elapsed;
    input.grind_substate = 2;
    v = vectors();
    state.update_exit_lean(input, &candidate, graph(), v);
    CHECK_EQ(state.elapsed, old_time);
    BalanceState state2 = BalanceState::default_();
    v = vectors();
    state2.update_exit_lean(exit_input(0.0f), &candidate, graph(), v);
    CHECK_EQ(state2.exit_direction, X);
}

S3_TEST(exit_history_retains_twenty_away_frames_then_resets_only_timers) {
    const GrindSurface geometry = surface(GeometryType::Ledge);
    BalanceState state = BalanceState::default_();
    const BalanceContact c = contact(geometry, 1, EntryKind::RideIntoCoping);
    BalanceVectors v = vectors();
    state.update_exit_lean(exit_input(0.2f), &c, graph(), v);
    BalanceVectors output{X, Z};
    for (std::int32_t count = 1; count <= 20; ++count) {
        state.update_exit_lean(exit_input(0.01f), nullptr, graph(), output);
        CHECK_EQ(state.frames_away, count);
        CHECK(state.elapsed > 0.0f);
    }
    CHECK(output == (BalanceVectors{X, Z}));
    state.previous_normal = Z;
    state.exit_direction = negate(X);
    state.update_exit_lean(exit_input(0.01f), nullptr, graph(), output);
    CHECK_EQ(state.elapsed, 0.0f);
    CHECK_EQ(state.exit_angle_degrees, 0.0f);
    CHECK_EQ(state.entry_delay, 2.0f);
    CHECK_EQ(state.previous_normal, Z);
    CHECK_EQ(state.exit_direction, negate(X));
    state.frames_away = INT32_MAX;
    state.elapsed = 1.0f;
    state.update_exit_lean(exit_input(0.01f), nullptr, graph(), output);
    CHECK_EQ(state.frames_away, INT32_MIN);
    CHECK(state.elapsed > 1.0f);  // native signed comparison, not unsigned/clamped
}

S3_TEST(grind_category_and_current_701_keep_existing_timer_alive_without_contact) {
    const std::pair<std::uint32_t, std::uint32_t> cases[] = {{400, 401}, {700, 701}};
    for (const auto& [category, current_state] : cases) {
        BalanceState state = BalanceState::default_();
        state.elapsed = 0.5f;
        state.frames_away = 20;
        ExitLeanInput input = exit_input(0.1f);
        input.category = category;
        input.current_state = current_state;
        BalanceVectors v = vectors();
        state.update_exit_lean(input, nullptr, graph(), v);
        CHECK_EQ(state.frames_away, 0);
        CHECK_EQ(state.elapsed, 0.6f);
    }
}

S3_TEST(exit_side_continuity_and_both_output_rotations_leave_history_unrotated) {
    GrindSurface geometry = surface(GeometryType::Ledge);
    BalanceState state = BalanceState::default_();
    BalanceVectors output = vectors();
    BalanceContact c = contact(geometry, 1, EntryKind::RideIntoCoping);
    const float angle = state.update_exit_lean(exit_input(0.6f), &c, graph(), output);
    CHECK(s3::rs::abs(angle - 5.0f) < 0.00001f);
    const float radians = s3::rs::to_radians(angle);
    const V expected{-s3::rs::sin(radians), s3::rs::cos(radians), 0.0f, 0.0f};
    close(output.grind_normal, expected);
    close(output.target_up, expected);
    CHECK_EQ(state.previous_normal, Y);
    geometry.high_side = negate(X);
    c = contact(geometry, 1, EntryKind::StayInGrind);
    BalanceVectors v = vectors();
    state.update_exit_lean(exit_input(0.0f), &c, graph(), v);
    close(state.exit_direction, X);
    // Thin rail chooses upmost cross primitive, not travel-directed direction.
    geometry.kind = GeometryType::ThinRail;
    BalanceContact candidate = contact(geometry, 0, EntryKind::StayInGrind);
    candidate.directed_grind_direction = negate(Z);
    v = vectors();
    state.update_exit_lean(exit_input(0.0f), &candidate, graph(), v);
    close(state.exit_direction, X);
    state.exit_direction = Z;  // exact zero alignment takes the negated side
    v = vectors();
    state.update_exit_lean(exit_input(0.0f), &candidate, graph(), v);
    close(state.exit_direction, negate(X));
}

using Hit = std::optional<ForceExitHit>;

S3_TEST(force_exit_thresholds_query_geometry_and_other_bits) {
    const V location{4.0f, 3.0f, 2.0f, 0.0f};
    struct Case {
        float angle;
        bool expected;
        int queries;
    };
    const Case cases[] = {{15.0f, false, 0}, {15.001f, true, 1}, {28.0f, true, 1}, {28.001f, true, 0}};
    for (const Case& c : cases) {
        BalanceState state = BalanceState::default_();
        state.exit_angle_degrees = c.angle;
        std::uint32_t flags = 0x81234567u;
        int calls = 0;
        const auto r = state.update_force_exit<s3::rs::Unit>(location, flags, [&](ForceExitProbe probe) {
            calls += 1;
            CHECK(probe == (ForceExitProbe{{3.9f, 3.0f, 2.0f, 0.0f}, {3.9f, -2.0f, 2.0f, 0.0f}}));
            return s3::rs::Result<Hit, s3::rs::Unit>(Hit{});
        });
        CHECK(r.is_ok());
        CHECK_EQ(calls, c.queries);
        CHECK_EQ(flags & 0x7fffffffu, 0x01234567u);
        CHECK_EQ((flags >> 31) != 0, c.expected);
    }
}

S3_TEST(force_exit_normal_comparison_is_strict_and_query_errors_preserve_flags) {
    BalanceState state = BalanceState::default_();
    state.exit_angle_degrees = 20.0f;
    const std::pair<float, bool> cases[] = {{-0.10001f, false}, {-0.1f, false}, {-0.09999f, true}};
    for (const auto& [normal_x, force] : cases) {
        std::uint32_t flags = 0x81234567u;
        const float nx = normal_x;
        const auto r = state.update_force_exit<s3::rs::Unit>({0.0f, 0.0f, 0.0f, 0.0f}, flags, [&](ForceExitProbe) {
            return s3::rs::Result<Hit, s3::rs::Unit>(Hit{ForceExitHit{{nx, 1.0f, 0.0f, 0.0f}}});
        });
        CHECK(r.is_ok());
        CHECK_EQ((flags >> 31) != 0, force);
    }
    std::uint32_t flags = 0x81234567u;
    const auto r = state.update_force_exit<std::string>({0.0f, 0.0f, 0.0f, 0.0f}, flags, [](ForceExitProbe) {
        return s3::rs::Result<Hit, std::string>(s3::rs::make_err(std::string("world unavailable")));
    });
    CHECK(r.is_err());
    CHECK_EQ(r.unwrap_err(), std::string("world unavailable"));
    CHECK_EQ(flags, 0x81234567u);
}
}  // namespace
