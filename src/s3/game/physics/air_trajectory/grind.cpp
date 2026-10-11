// Port of skate-game/src/physics/air_trajectory/grind.rs.
//! Real static University provider -> native trajectory admission -> landing orientation.
#include "s3/game/physics/air_trajectory/grind.hpp"

#include "s3/core/air/trajectory/grind_surface.hpp"
#include "s3/game/physics/player_input/grind/world.hpp"

namespace s3::game::physics::air_trajectory::grind {

namespace grind_surface = ::s3::air::trajectory::grind_surface;
namespace tgrind = ::s3::air::trajectory::grind;
using grind_surface::GeometryType;
using grind_surface::InvestigationInput;
using grind_surface::LandingOrientation;
using ::s3::point_graph::PointGraph;
using tgrind::GrindSurfaceEvidence;
using tgrind::GrindTarget;
using tgrind::GrindTrajectoryCandidate;

GrindContext GrindContext::from_processed(const ProcessedPhysicsInput& p, V board_position) {
    GrindContext c;
    c.board_position = board_position;
    c.body_position = rs::from_bits(p.vectors_544_560_592_608[2]);
    c.actor = {p.actor_query_2948, p.actor_query_2952};
    return c;
}

rs::Result<Settings, std::string> Settings::load(const sd::Collections& data) {
    auto t = [&](const char* key) { return data.float_("physics_trajectory", "default", key); };
    Settings s;
    s.limits.lock_distance = 0.0f;
    S3G_TRY(s.limits.max_speed_squared_ledge, t("GrindMaxSpeedSqrIntoLedge"));
    S3G_TRY(s.limits.max_speed_squared_rail, t("GrindMaxSpeedSqrIntoGrind"));
    S3G_TRY(s.limits.max_downward_speed, t("GrindMaxSpeedDownOntoGrind"));
    S3G_TRY(s.limits.ledge_scalars[0], t("GrindLockLedgeScalar2"));
    S3G_TRY(s.limits.ledge_scalars[1], t("GrindLockLedgeScalar"));
    S3G_TRY(s.limits.ledge_scalars[2], t("GrindLockLedgeLowSideScalar2"));
    S3G_TRY(s.limits.ledge_scalars[3], t("GrindLockLedgeLowSideScalar"));
    S3G_TRY(s.limits.tip_scalar, t("GrindTipScalar"));
    S3G_TRY(s.limits.maximum_adjust_angle, t("GrindAdjustMaxAngle"));
    S3G_TRY(s.limits.deck_dimensions[0], data.float_("physicsdeck", "default", "DeckMidLength"));
    S3G_TRY(s.limits.deck_dimensions[1], data.float_("physicsdeck", "default", "DeckFrontEndSize"));
    S3G_TRY(s.height, graph8(data, "physics_trajectory", "RequiredAngleVsHeight", true));
    S3G_TRY(s.padding, t("GrindOffset"));
    S3G_TRY(s.maximum_adjust, t("MaxTrajectoryAdjust"));
    S3G_TRY(s.velocity_scalar, t("GrindLandingVelScalar"));
    S3G_TRY(s.max_angle, t("GrindLandingMaxAngle"));
    S3G_TRY(s.score, t("ScoreGrind"));
    S3G_TRY(s.truck_distance, data.float_("physics_grinds", "default", "DeckCenterToTruck"));
    S3G_TRY(const auto words, data.words<20>("physics_trajectory", "default", "GrindPenaltyVsDistToGrind"));
    s.penalty_domain = rs::from_bits(words[2]);
    return s;
}

rs::Result<GrindEvaluation, std::string> Settings::evaluate(Prediction& prediction, bool acquire,
                                                            const BoardWorld& world, const StaticProvider& provider,
                                                            std::vector<std::size_t>& nearby, float lock_distance,
                                                            GrindContext context) const {
    const V collision_position = prediction.collision_position();
    const V collision_velocity = prediction.collision_velocity();
    if (acquire) {
        std::array<float, 3> min{};
        std::array<float, 3> max{};
        for (std::size_t i = 0; i < 3; ++i) min[i] = collision_position[i] - (i == 1 ? 0.5f : 2.0f);
        for (std::size_t i = 0; i < 3; ++i) max[i] = collision_position[i] + (i == 1 ? 4.0f : 2.0f);
        S3G_TRY(const auto indices, provider.query(min, max));
        nearby = tgrind::candidate::trajectory_box_filter(indices, provider.primitives(), context.board_position);
    }
    GrindEvaluation result;
    result.target = std::nullopt;
    result.score = 0.0f;
    result.distance = 1000.0f;
    result.effective_lock_distance = std::nullopt;
    result.penalty_domain = penalty_domain;
    //82D69D68: retain the last real query for non-acquisition candidates.
    float square = 1000000.0f;
    for (const std::size_t index : nearby) {
        const auto edge = provider.primitives()[index];
        const V axis = unit(sub(edge.end, edge.start));
        const V from = sub(collision_position, edge.start);
        const V end = sub(collision_position, edge.end);
        const V perpendicular = sub(from, scale(axis, dot(axis, from)));
        square = rs::min(rs::min(rs::min(dot(perpendicular, perpendicular), dot(from, from)), dot(end, end)), square);
    }
    if (!nearby.empty()) {
        result.distance = length_square(square);
    }
    if (!acquire || nearby.empty()) {
        return result;
    }
    std::vector<GrindTrajectoryCandidate> candidates;
    for (const std::size_t i : nearby) {
        if (auto c = tgrind::candidate::consider_grind_primitive(prediction, provider.primitives()[i], i, padding)) {
            candidates.push_back(*c);
        }
    }
    auto limits_ = limits;
    limits_.lock_distance = lock_distance;
    while (const auto best = tgrind::candidate::take_best_grind(candidates, lock_distance, height)) {
        const auto c = *best;
        const auto edge = provider.primitives()[c.primitive];
        InvestigationInput query;
        query.start = edge.start;
        query.end = edge.end;
        query.reference = c.point;
        query.optional_probe = std::nullopt;
        query.deck_center_to_truck = truck_distance;
        //No submission is a real failed candidate, not an INVALID completed fallback.
        if (!grind_surface::prepare(query)) {
            continue;
        }
        S3G_TRY(const auto surface,
                (grind_surface::investigate<std::string>(query, [&](std::size_t index, grind_surface::Probe probe) {
                    return ::s3::game::physics::player_input::grind::world::surface_probe(world, context.actor,
                                                                                          index, probe);
                })));
        //All fields below are written by the completed producer before consumption.
        LandingOrientation orientation;
        orientation.kind = GeometryType::Impossible;
        orientation.garbage = false;
        orientation.boardslide_dir = {0.0f, 0.0f, 0.0f, 0.0f};
        orientation.tipslide_dir = {0.0f, 0.0f, 0.0f, 0.0f};
        orientation.backslash_dir = {0.0f, 0.0f, 0.0f, 0.0f};
        orientation.high_side = {0.0f, 0.0f, 0.0f, 0.0f};
        V support{0.0f, 0.0f, 0.0f, 0.0f};
        grind_surface::update_landing_orientation(&surface, context.board_position, c.point, support, orientation);
        const auto delta = tgrind::admitted_displacement(
            prediction, c, edge, collision_velocity, context.body_position,
            GrindSurfaceEvidence{static_cast<std::uint32_t>(orientation.kind), orientation.high_side}, limits_,
            result.effective_lock_distance);
        if (!delta) {
            continue;
        }
        const V original_velocity = prediction.request.trajectory.velocity;
        tgrind::apply_admitted_target(prediction, c, *delta, support, original_velocity, maximum_adjust,
                                      velocity_scalar, max_angle);
        V vertical = cross(cross(c.direction, V{0.0f, 1.0f, 0.0f, 0.0f}), c.direction);
        vertical = vertical[1] < 0.0f ? scale(vertical, -1.0f) : vertical;
        vertical = length_square(dot(vertical, vertical)) > rs::from_bits(0x358637bdu) ? unit(vertical)
                                                                                         : V{0.0f, 1.0f, 0.0f, 0.0f};
        const auto* metadata = provider.metadata(c.primitive);
        if (metadata == nullptr) {
            return rs::make_err(std::string("Accepted grind primitive lacks native metadata"));
        }
        const std::uint32_t primitive_flags = metadata->flags;
        GrindTarget target;
        target.edge = edge;
        target.provider_index = c.primitive;
        target.primitive_flags = primitive_flags;
        target.orientation = orientation;
        target.point = c.point;
        target.vertical_normal = vertical;
        result.target = target;
        result.score = score;
        return result;
    }
    return result;
}

float dot(V a, V b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

rs::Result<PointGraph<8>, std::string> graph8(const sd::Collections& data, const std::string& class_,
                                             const std::string& name, bool negative) {
    const char* expected = negative ? "Sk8::PointNegGraphData8" : "Sk8::PointGraphData8";
    S3G_TRY(const sd::Field* field, data.field(class_, "default", name));
    if (field->type_name != expected) {
        return rs::make_err(fmt::cat("Wrong stock graph type ", class_, "/", name));
    }
    S3G_TRY(const auto words, data.words<20>(class_, "default", name));
    PointGraph<8> g;
    for (std::size_t i = 0; i < 8; ++i) {
        g.x[i] = rs::from_bits(words[4 + i]);
        g.y[i] = rs::from_bits(words[12 + i]);
    }
    return g;
}

V sub(V a, V b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }

V scale(V a, float s) { return {a[0] * s, a[1] * s, a[2] * s, a[3] * s}; }

V cross(V a, V b) {
    return {
        std::fma(-a[2], b[1], a[1] * b[2]),
        std::fma(-a[0], b[2], a[2] * b[0]),
        std::fma(-a[1], b[0], a[0] * b[1]),
        0.0f,
    };
}

float inverse(float square) {
    float r = rs::recip(rs::sqrt(square));
    for (int i = 0; i < 2; ++i) {
        r = std::fma(r * 0.5f, std::fma(-square, r * r, 1.0f), r);
    }
    return r;
}

float length_square(float square) { return square == 0.0f ? 0.0f : square * inverse(square); }

V unit(V a) {
    const float q = dot(a, a);
    if (length_square(q) > rs::from_bits(0x358637bdu)) {
        return scale(a, inverse(q));
    }
    return {0.0f, 0.0f, 0.0f, 0.0f};
}

}  // namespace s3::game::physics::air_trajectory::grind
