// Port of skate-core/src/physics/tests/board_ground.rs.
#include <optional>
#include <vector>

#include "harness.hpp"
#include "s3/core/physics/board_ground.hpp"

namespace {
using namespace s3::physics::board_ground;
using s3::physics::board_step::CollisionBody;

using Hits = std::array<std::optional<WheelLineHit>, 4>;
Hits all(std::optional<WheelLineHit> h) { return {h, h, h, h}; }
const std::vector<BoardContactReport> NONE{};

BoardContactReport report(BodyId part, Vector3 normal) {
    BoardContactReport r;
    r.part = part;
    r.other = CollisionBody::StaticWorld();
    r.is_body_a = true;
    r.normal = normal;
    r.position = Vector3::ZERO;
    r.relative_linear_velocity = Vector3::ZERO;
    r.other_surface = 0;
    r.normal_force_on_a = UP;
    r.friction_force_on_a = Vector3::ZERO;
    r.tangents = {Vector3::ZERO, Vector3::ZERO};
    return r;
}

/// Regression for the paired postphysics branch contract, not fidelity proof.
/// S2 82B372E0 names mAngularDrag; S3 82C08634 stores inertia+36.
S3_TEST(wheel_angular_drag_tracks_physical_contact_and_wipeout) {
    BoardGroundState ground = BoardGroundState::default_();
    WheelLineState lines = WheelLineState::default_();
    // A successful line query is not physical wheel contact for drag selection.
    lines.publish(all(WheelLineHit{0.1f, UP, 0}));
    const float frequency = s3::rs::from_bits(0x426FFFFFu);
    const float free = s3::rs::from_bits(0x3BC49BA6u) * frequency;
    const float wiping_out = s3::rs::from_bits(0x3D23D70Au) * frequency;
    const std::vector<BoardContactReport> contacts{report(BodyId::RightFrontWheel, UP),
                                                   report(BodyId::LeftBackWheel, UP)};
    ground.update(contacts, lines, UP, 80.0f, false);
    CHECK_EQ(ground.wheel_angular_drag, (std::array<float, 4>{0.0f, free, free, 0.0f}));
    ground.update(contacts, lines, UP, 80.0f, true);
    CHECK_EQ(ground.wheel_angular_drag, (std::array<float, 4>{wiping_out, free, free, wiping_out}));
    ground.update(NONE, lines, UP, 80.0f, true);
    CHECK_EQ(ground.wheel_angular_drag, (std::array<float, 4>{free, free, free, free}));
    ground.update(contacts, lines, UP, 80.0f, false);
    CHECK_EQ(ground.wheel_angular_drag, (std::array<float, 4>{0.0f, free, free, 0.0f}));
}

S3_TEST(lost_wheel_support_keeps_wheel_normal_but_rebuilds_overall_normal) {
    BoardGroundState ground = BoardGroundState::default_();
    WheelLineState lines = WheelLineState::default_();
    lines.publish(all(WheelLineHit{0.1f, UP, 0}));
    ground.update(std::vector<BoardContactReport>{report(BodyId::RightFrontWheel, UP)}, lines, UP, 80.0f, false);
    CHECK_EQ(ground.wheel_contact_count, std::uint8_t(1));
    const Vector3 retained = ground.wheel_normal;
    ground.update(std::vector<BoardContactReport>{report(BodyId::Deck, Vector3{1.0f, 0.0f, 0.0f})}, lines, UP, 80.0f,
                  false);
    CHECK_EQ(ground.wheel_contact_count, std::uint8_t(0));
    CHECK_EQ(ground.part_contact_count, std::uint8_t(1));
    CHECK_EQ(ground.wheel_normal, retained);
    CHECK(ground.overall_normal.x > 0.99999f);
    CHECK(s3::rs::abs(ground.overall_normal.y) < 0.00001f);
}

S3_TEST(no_hit_preserves_line_geometry_and_contact_status_does_not_come_from_lines) {
    WheelLineState lines = WheelLineState::default_();
    const Vector3 normal{0.6f, 0.8f, 0.0f};
    lines.publish(all(WheelLineHit{0.2f, normal, 42u | (3u << 7)}));
    CHECK_EQ(lines.audio_surfaces, (std::array<std::uint32_t, 4>{42, 42, 42, 42}));
    CHECK_EQ(lines.physics_surfaces, (std::array<std::uint32_t, 4>{3, 3, 3, 3}));
    const auto distances = lines.distances;
    lines.publish(all(std::nullopt));
    CHECK_EQ(lines.normals, (std::array<Vector3, 4>{normal, normal, normal, normal}));
    CHECK_EQ(lines.distances, distances);
    CHECK_EQ(lines.audio_surfaces, (std::array<std::uint32_t, 4>{}));
    CHECK_EQ(lines.physics_surfaces, (std::array<std::uint32_t, 4>{}));
    CHECK_EQ(lines.minimum_distance, WHEEL_LINE_LENGTH);
    BoardGroundState ground = BoardGroundState::default_();
    ground.update(NONE, lines, UP, 80.0f, false);
    CHECK_EQ(ground.part_contact_count, std::uint8_t(0));
    CHECK_EQ(ground.wheel_contact_count, std::uint8_t(0));
    CHECK_EQ(ground.wheel_normal, UP);
    CHECK_EQ(ground.overall_normal, UP);
}

S3_TEST(per_part_selection_uses_highest_y_and_keeps_first_on_ties) {
    const auto first = report(BodyId::Deck, Vector3{0.6f, 0.8f, 0.0f});
    const auto tied = report(BodyId::Deck, Vector3{-0.6f, 0.8f, 0.0f});
    const auto lower = report(BodyId::Deck, Vector3{0.0f, 0.4f, 0.9f});
    BoardGroundState ground = BoardGroundState::default_();
    ground.update(std::vector<BoardContactReport>{first, tied, lower}, WheelLineState::default_(), UP, 80.0f, false);
    CHECK_EQ(ground.parts[6].normal, first.normal);
    CHECK_EQ(ground.part_contact_count, std::uint8_t(1));
}

S3_TEST(observed_acceleration_retains_each_part_velocity_and_resets_with_collision_info) {
    BoardGroundState state = BoardGroundState::default_();
    std::array<Vector3, BODY_COUNT> first{};
    for (std::size_t i = 0; i < BODY_COUNT; ++i) first[i] = Vector3{static_cast<float>(i), 2.0f, -1.0f};
    state.sample_accelerations(first, 0.5f);
    CHECK_EQ(state.accelerations[6], (Vector3{12.0f, 4.0f, -2.0f}));
    state.sample_accelerations(first, 0.25f);
    CHECK_EQ(state.accelerations, (std::array<Vector3, 7>{}));
    auto next = first;
    next[6].x += 1.0f;
    state.sample_accelerations(next, 0.25f);
    CHECK_EQ(state.accelerations[6], (Vector3{4.0f, 0.0f, 0.0f}));
    CHECK_EQ(state.accelerations[5], Vector3::ZERO);
    const BoardGroundState reset = BoardGroundState::default_();
    CHECK_EQ(reset.previous_velocities, (std::array<Vector3, 7>{}));
    CHECK_EQ(reset.accelerations, (std::array<Vector3, 7>{}));
}

S3_TEST(impact_uses_preceding_velocity_even_after_the_solver_stops_the_board) {
    BoardGroundState ground = BoardGroundState::default_();
    std::array<Vector3, BODY_COUNT> before{};
    before[6] = Vector3{-8.0f, -3.0f, 0.0f};
    ground.sample_accelerations(before, 1.0f / 60.0f);
    const auto wall = report(BodyId::Deck, Vector3{1.0f, 0.0f, 0.0f});
    const auto floor = report(BodyId::Deck, UP);
    // Both actual solver reports have zero remaining relative velocity. The
    // wall still has the larger incoming impact, while the floor wins normal Y.
    ground.update(std::vector<BoardContactReport>{wall, floor}, WheelLineState::default_(), UP, 80.0f, false);
    ground.sample_accelerations(std::array<Vector3, BODY_COUNT>{}, 1.0f / 60.0f);
    CHECK_EQ(ground.closing_velocity, (Vector3{-8.0f, -0.0f, -0.0f}));
    CHECK_EQ(ground.maximum_closing_speed, 8.0f);
    CHECK_EQ(ground.parts[6].normal, UP);
    ground.update(NONE, WheelLineState::default_(), UP, 80.0f, false);
    CHECK_EQ(ground.closing_velocity, Vector3::ZERO);
    CHECK_EQ(ground.maximum_closing_speed, 0.0f);
}

S3_TEST(surface_flags_use_actual_contacts_and_reset_each_frame) {
    BoardGroundState ground = BoardGroundState::default_();
    auto first = report(BodyId::Deck, UP);
    first.other_surface = 12 << 7;
    first.position.y = 2.0f;
    auto last = first;
    last.position.y = 5.0f;
    WheelLineState lines = WheelLineState::default_();
    lines.physics_surfaces[0] = 8;
    ground.update(std::vector<BoardContactReport>{first, last}, lines, UP, 80.0f, false);
    CHECK_EQ(ground.collision_flags, 1u << 25);
    CHECK_EQ(ground.surface_twelve_height, 5.0f);
    // A wheel ray reporting surface8 alone does not set the physical flag.
    ground.update(std::vector<BoardContactReport>{report(BodyId::RightFrontWheel, UP)}, lines, UP, 80.0f, false);
    CHECK_EQ(ground.collision_flags, 1u << 31);
    ground.update(NONE, lines, UP, 80.0f, false);
    CHECK_EQ(ground.collision_flags, 0u);
    CHECK_EQ(ground.surface_twelve_height, 0.0f);
}

S3_TEST(opposing_contacts_use_deck_projection_range_and_reset_without_deck_contacts) {
    BoardGroundState state = BoardGroundState::default_();
    const WheelLineState lines = WheelLineState::default_();
    const std::vector<BoardContactReport> reports{
        report(BodyId::Deck, Vector3{0.0f, 0.75f, 0.0f}),
        report(BodyId::Deck, Vector3{0.0f, -0.5f, 0.0f}),
        report(BodyId::FrontTruck, Vector3{0.0f, -1.0f, 0.0f}),
    };
    state.update(reports, lines, UP, 80.0f, false);
    CHECK_EQ(state.opposing_contact, 1.25f);
    state.update(reports, lines, Vector3{1.0f, 0.0f, 0.0f}, 80.0f, false);
    CHECK_EQ(state.opposing_contact, 0.0f);
    state.update(std::vector<BoardContactReport>(reports.begin() + 2, reports.end()), lines, UP, 80.0f, false);
    CHECK_EQ(state.opposing_contact, 0.0f);
}

/// 82C079E0 keeps tag & 0x7F, (tag >> 7) & 31 and (tag >> 12) & 0xF per wheel and clears all
/// three on a miss; 82C07D20 keeps the audio/seam bits of truck/deck reports (last wins) after
/// Reset 82C00CA0 cleared them.
S3_TEST(audio_material_and_seam_bits_are_preserved_from_the_same_tags) {
    const std::uint32_t tag = (0xBu << 12) | (3u << 7) | 0x45u;
    WheelLineState lines = WheelLineState::default_();
    lines.publish(Hits{WheelLineHit{0.1f, UP, tag}, std::nullopt, std::nullopt, std::nullopt});
    CHECK_EQ(lines.audio_surfaces, (std::array<std::uint32_t, 4>{0x45, 0, 0, 0}));
    CHECK_EQ(lines.physics_surfaces, (std::array<std::uint32_t, 4>{3, 0, 0, 0}));
    CHECK_EQ(lines.seam_patterns, (std::array<std::uint32_t, 4>{0xB, 0, 0, 0}));
    lines.publish(all(std::nullopt));
    CHECK_EQ(lines.seam_patterns, (std::array<std::uint32_t, 4>{}));

    BoardGroundState ground = BoardGroundState::default_();
    auto first = report(BodyId::Deck, UP);
    first.other_surface = static_cast<std::uint16_t>((0x4u << 12) | 0x02u);
    auto last = report(BodyId::Deck, UP);
    last.other_surface = static_cast<std::uint16_t>((0x8u << 12) | (2u << 7) | 0x10u);
    auto truck = report(BodyId::BackTruck, UP);
    truck.other_surface = 0x03;
    ground.update(std::vector<BoardContactReport>{first, last, truck}, lines, UP, 80.0f, false);
    CHECK_EQ(ground.part_audio_surfaces, (std::array<std::uint32_t, 3>{0, 0x03, 0x10}));
    CHECK_EQ(ground.part_seam_patterns, (std::array<std::uint32_t, 3>{0, 0, 0x8}));
    ground.update(NONE, lines, UP, 80.0f, false);
    CHECK_EQ(ground.part_audio_surfaces, (std::array<std::uint32_t, 3>{}));
    CHECK_EQ(ground.part_seam_patterns, (std::array<std::uint32_t, 3>{}));
}
}  // namespace
