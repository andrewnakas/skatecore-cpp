// Port of skate-game/src/physics/player_input/grind/pre.rs.
#include "s3/game/physics/player_input/grind/pre.hpp"

#include "s3/core/physics/grind_contact/admission.hpp"
#include "s3/core/physics/grind_contact/investigator.hpp"

namespace s3::game::physics::player_input::grind {

using sc::physics::grind_contact::admission::Admission;
namespace investigator = sc::physics::grind_contact::investigator;

rs::Result<Pending, std::string> GrindInputState::pre_update(const ProcessedPhysicsInput& p,
                                                             const StaticProvider& provider, const BoardWorld& world,
                                                             PreContext context, Host& host) {
    previous_direction = float_(investigation.direction_1136);
    investigation = GrindInvestigationFields{};
    permission(p, context.air_counter);
    std::vector<std::size_t> indices;
    if ((p.flags_2476 & 0x01000000u) == 0) {
        std::array<float, 3> min{};
        std::array<float, 3> max{};
        for (std::size_t i = 0; i < 3; ++i) min[i] = context.board[3][i] - 1.2f;
        for (std::size_t i = 0; i < 3; ++i) max[i] = context.board[3][i] + 1.2f;
        S3G_TRY(indices, provider.query(min, max));
    }
    const MaterialMode mode = material_mode(p, !indices.empty(), context.air_targeting_grind_9653);
    S3G_CHECK(host.apply_material_mode(mode));
    advance_history(p);
    std::vector<sc::physics::grind_contact::Primitive> edges;
    edges.reserve(indices.size());
    for (const std::size_t i : indices) edges.push_back(provider.primitives()[i]);
    investigator::Query query;
    query.board = context.board;
    query.admission = Admission{p.category_2512, p.state_2508, p.scalar_2652, float_(p.vectors_400_416[0]),
                                &settings.slope_threshold};
    query.flags_2468 = p.flags_2468;
    query.flags_2472 = p.flags_2472;
    query.flags_2476 = p.flags_2476;
    query.flags_2484 = p.flags_2484;
    query.tip_state = context.tip_state;
    query.truck_to_wheel = settings.truck_to_wheel;
    query.deck_to_truck = settings.deck_to_truck;
    query.test_above = settings.test_above;
    query.test_below = settings.test_below;
    query.translation = context.translation_2796;
    query.stability_nudge = context.stability_nudge_2800;
    query.balance = context.balance_2720;
    query.reference_right = float_(p.vectors_464_480_496_512_528[0]);
    query.ground_frames = grounded_frames;
    query.low_wheel_frames = low_wheel_frames;
    query.forbidden = disabled || (grind_history != 0 && p.grind_words_2532_2536[1] != 2);
    const auto result = investigator::investigate(query, edges);
    GrindInvestigationFields fields{};
    std::optional<PrimitiveMetadata> metadata;
    std::optional<GeometryWork> geometry;
    if (result.candidate) {
        const auto& contact = *result.candidate;
        const auto& g = contact.geometry;
        const auto edge = edges[g.primitive];
        const auto* native = provider.metadata(indices[g.primitive]);
        if (native == nullptr) {
            return rs::make_err(std::string("Selected static grind primitive lacks metadata"));
        }
        metadata = PrimitiveMetadata{native->spline_guids};
        fields.valid_1488 = true;
        fields.family_1248 = contact.kind;
        fields.entry_kind_1252 = static_cast<std::uint32_t>(contact.entry_kind);
        fields.tangent_1104 = raw(g.direction);
        fields.point_1120 = raw(g.centre);
        fields.direction_1136 =
            raw(manager::directed_tangent(edge.start, edge.end, float_(p.vectors_400_416[0]), previous_direction));
        fields.primitive_start_1264 = raw(edge.start);
        fields.primitive_end_1280 = raw(edge.end);
        fields.owner_1296 = edge.owner;
        fields.primitive_flags_1300 = native->flags & 0x80000000u;
        fields.flags_1516 = contact.front ? 0x20000000u : 0u;
        // 82D8939C/A0 writes both truck hits. FiveO82D89CB8/9D5C
        // writes the selected hit to+96 regardless of front/rear selection.
        if (contact.kind == 0) {
            fields.front_contact_1200 = raw(g.front);
            fields.rear_contact_1216 = raw(g.rear);
        } else if (contact.kind == 3) {
            fields.front_contact_1200 = raw(g.centre);
        }
        grind_surface::InvestigationInput input;
        input.start = edge.start;
        input.end = edge.end;
        input.reference = g.centre;
        input.optional_probe = p.state_2508 == 402 ? std::optional<V>(context.board[3]) : std::nullopt;
        input.deck_center_to_truck = settings.deck_to_truck;
        auto plan = grind_surface::prepare(input);
        GeometryWork work;
        work.input = input;
        work.plan = std::move(plan);
        work.hits = {};
        geometry = std::move(work);
        // Accepted family returns before investigator's history-byte write.
    } else {
        previous_proximity = result.proximity.has_value();
        if (result.proximity) {
            const auto& proximity = *result.proximity;
            const auto edge = edges[proximity.contact.primitive];
            const auto* native = provider.metadata(indices[proximity.contact.primitive]);
            if (native == nullptr) {
                return rs::make_err(std::string("Static grind proximity lacks metadata"));
            }
            // 82D87E18/34/3C: contact+128 and SECOND primitive+208/+224.
            fields.vector_1232 = raw(proximity.contact.position);
            fields.second_start_1312 = raw(edge.start);
            fields.second_end_1328 = raw(edge.end);
            fields.second_owner_1344 = edge.owner;
            fields.second_flags_1348 = native->flags & 0x80000000u;
            fields.flags_1516 = 0x08000000u | (proximity.deck_contact ? 0x04000000u : 0u);
        }
    }
    previous_velocity = p.vectors_400_416[0];
    friction_vs_time = settings.friction.evaluate(elapsed);
    fields.friction_1496 = friction_vs_time;
    investigation = fields;
    // 82D8AA54..82D8AAF8 publishes previous velocity and friction before
    // submitting geometry descriptors. Post resolves these exact results.
    if (geometry && geometry->plan) {
        const auto& probes = geometry->plan->probes;
        for (std::size_t index = 0; index < probes.size(); ++index) {
            S3G_TRY(geometry->hits.at(index),
                    host.surface_probe(world, {p.actor_query_2948, p.actor_query_2952}, index, probes[index]));
        }
    }
    Pending pending;
    pending.fields = fields;
    pending.geometry = std::move(geometry);
    pending.metadata = metadata;
    return pending;
}

}  // namespace s3::game::physics::player_input::grind
