// Port of skate-game/src/physics/player_input/grind/post.rs.
#include "s3/game/physics/player_input/grind/post.hpp"

#include "s3/game/physics/player_input/grind/publication.hpp"

namespace s3::game::physics::player_input::grind {

using sc::physics::grind_contact::admission::EntryKind;

rs::Result<PostResult, std::string> GrindInputState::post_update(ProcessedPhysicsInput& p, const BoardWorld& world,
                                                                 Pending pending, PostContext context, Host& host) {
    GrindInvestigationFields fields = pending.fields;
    std::optional<GeometryWork> geometry = std::move(pending.geometry);
    const std::optional<PrimitiveMetadata> metadata = pending.metadata;
    const std::array<std::uint32_t, 2> actor = {p.actor_query_2948, p.actor_query_2952};
    std::optional<GrindSurface> surface;
    if (geometry) {
        GeometryWork& work = *geometry;
        GrindSurface resolved;
        if (work.plan) {
            resolved = grind_surface::resolve(*work.plan, work.hits, fields.geometry_flags_1476);
        } else {
            // The original inadequate-up gate returns its specific
            // not-submitted result. Reuse the core branch, not a fake miss.
            S3G_TRY(resolved, (grind_surface::investigate<std::string>(work.input, [&](std::size_t index, Probe probe) {
                        return host.surface_probe(world, actor, index, probe);
                    })));
        }
        publication::surface(fields, resolved);
        manager::GeometryInput gi;
        gi.valid = fields.valid_1488;
        gi.family = fields.family_1248;
        gi.current_state = p.state_2508;
        gi.category = p.category_2512;
        gi.air_frames = air_frames;
        gi.geometry_kind = static_cast<std::uint32_t>(resolved.kind);
        gi.geometry_flags = resolved.flags;
        gi.far_points = resolved.far_points;
        gi.upmost = resolved.upmost_normal;
        gi.high_side = resolved.high_side;
        gi.point = float_(fields.point_1120);
        gi.direction = float_(fields.direction_1136);
        gi.board_position = context.board[3];
        gi.board_forward = context.board[2];
        gi.velocity = float_(p.vectors_400_416[0]);
        gi.deck_to_truck = settings.deck_to_truck;
        gi.previous_exit_angle = balance.exit_angle_degrees;
        gi.previous_exit_direction = balance.exit_direction;
        gi.flags = fields.flags_1516;
        const auto output = manager::tweak_geometry(gi, secondary_history);
        fields.valid_1488 = output.valid;
        fields.family_1248 = output.family;
        fields.flags_1516 = output.flags;
        surface = resolved;
    }

    fields.gravity_relief_1512 =
        manager::gravity_relief(gravity_timer, fields.valid_1488, p.category_2512, float_(fields.tangent_1104),
                                float_(p.vectors_400_416[0]), p.timestep_2604, settings.gravity_vertical,
                                settings.gravity_linear);
    std::optional<balance::BalanceContact> contact;
    if (surface && fields.valid_1488) {
        S3G_TRY(const EntryKind kind, post::entry_kind(fields.entry_kind_1252));
        balance::BalanceContact bc;
        bc.surface = &*surface;
        bc.primitive_direction = float_(fields.tangent_1104);
        bc.directed_grind_direction = float_(fields.direction_1136);
        bc.kind = fields.family_1248;
        bc.entry_kind = kind;
        contact = bc;
    }
    const balance::BalanceContact* contact_ptr = contact ? &*contact : nullptr;
    // Zeroed each pre frame by the original investigation reset. Invalid
    // target-up updates leave these zeros; history lives in BalanceState.
    balance::BalanceVectors vectors{float_(fields.normal_1152), float_(fields.target_up_1168)};
    balance.update_target_up(balance::TargetUpInput{p.category_2512, p.state_2504,
                                                    float_(p.vectors_544_560_592_608[0])},
                             contact_ptr, vectors);
    fields.exit_lean_1500 = balance.update_exit_lean(
        balance::ExitLeanInput{p.category_2512, p.state_2508, p.grind_words_2532_2536[1], p.timestep_2604},
        contact_ptr, settings.exit_lean, vectors);
    fields.normal_1152 = raw(vectors.grind_normal);
    fields.target_up_1168 = raw(vectors.target_up);
    S3G_CHECK((balance.update_force_exit<std::string>(
        float_(fields.point_1120), fields.flags_1516,
        [&](balance::ForceExitProbe probe) { return host.force_exit_line(world, actor, probe); })));

    entry::Input ei;
    ei.valid = fields.valid_1488;
    ei.kind = fields.family_1248;
    ei.category = p.category_2512;
    ei.previous_state_2504 = p.state_2504;
    ei.speed = p.scalar_2652;
    ei.balance_2720 = context.balance_2720;
    ei.direction = float_(fields.direction_1136);
    ei.normal = vectors.grind_normal;
    ei.up = float_(p.vectors_544_560_592_608[0]);
    ei.board_velocity = float_(p.vectors_400_416[0]);
    ei.air_velocity = float_(p.vectors_544_560_592_608[3]);
    ei.surface_kind = fields.geometry_kind_1464;
    ei.high_side = float_(fields.high_side_1440);
    ei.vertical_help = &settings.vertical_help;
    ei.max_delta = settings.max_impact;
    ei.flags = fields.flags_1516;
    ei.previous_entry_velocity = float_(fields.entry_velocity_1184);
    auto engagement_out = engagement.update(ei);
    fields.valid_1488 = engagement_out.valid;
    fields.flags_1516 = engagement_out.flags;
    fields.entry_velocity_1184 = raw(engagement_out.entry_velocity);
    fields.impact_speed_1492 = engagement_out.impact_speed;
    if (!engagement_out.wipeout_reasons.empty()) {
        p.flags_2468 |= 0x00040000u;
    }
    // Control::update's other-family branch is exactly the invalid-candidate
    // zero-target smoothing branch. Do not feed reset family0 as a50-50.
    control.update(fields.valid_1488 ? fields.family_1248 : UINT32_MAX, context.board[2], vectors.grind_normal,
                   (fields.flags_1516 & 0x20000000u) != 0, (p.flags_2468 & 0x00100000u) != 0,
                   (fields.flags_1516 & 0x10000000u) != 0, context.translation_2796, context.stability_nudge_2800,
                   context.up_down_2804, context.grab_min_height_2808);
    fields.yaw_1504 = control.yaw;
    fields.pitch_1508 = control.pitch;
    std::optional<manager::JumpGeometry> jump_geometry;
    if (fields.valid_1488) {
        manager::JumpGeometry jg;
        jg.geometry_kind = fields.geometry_kind_1464;
        jg.high_side = float_(fields.high_side_1440);
        jg.normal = vectors.grind_normal;
        jg.direction = float_(fields.direction_1136);
        jg.upmost = float_(fields.upmost_normal_1408);
        jg.point = float_(fields.point_1120);
        jump_geometry = jg;
    }
    p.flags_2476 |= jumper.update(jump_geometry, p.grind_words_2532_2536[0]);
    investigation = fields;
    p.grind = fields;
    S3G_TRY(auto observation, publication::observation(p, context, metadata, jumper));
    PostResult result;
    result.wipeout_reasons = std::move(engagement_out.wipeout_reasons);
    result.observation = observation;
    return result;
}

namespace post {

rs::Result<EntryKind, std::string> entry_kind(std::uint32_t value) {
    // Only reset0 or a typed investigator decision reaches this adapter.
    switch (value) {
        case 0: return EntryKind::RideFromAbove;
        case 1: return EntryKind::RideFromBelow;
        case 2: return EntryKind::RideIntoCoping;
        case 3: return EntryKind::StayInGrind;
        case 4: return EntryKind::ChangeGrind;
        case 5: return EntryKind::AirToGrind;
        case 6: return EntryKind::DropIn;
        default: return rs::make_err(fmt::cat("Invalid published grind admission kind ", value));
    }
}

}  // namespace post

}  // namespace s3::game::physics::player_input::grind
