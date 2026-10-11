// Port of skate-core/src/player/offboard/air_selector.rs and its adjustment.rs,
// candidates.rs and completion.rs submodules.
#include "s3/core/player/offboard/air_selector.hpp"

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::air_selector {

using namespace math;

// ------------------------------------------------------------------ air_selector.rs
Selector Selector::new_(const Packet& launch) {
    // Constructor82D6B870 differs from Reset82D6BD78 at these fields.
    SelectorState sampling = SelectorState::reset_sampling(launch.scalar_100);
    sampling.restart_allowed_8493 = false;
    sampling.selection.landing_frame_8480 = 0;
    Selector s;
    s.sampling = sampling;
    s.launch = launch;
    s.selected_index = std::nullopt;
    s.selected_candidate = Candidate::reset();
    s.offset_8272 = Vector{};
    s.correction_8304 = Vector{};
    s.ledge_normal_8320 = UP;
    s.ledge_selected_8496 = false;
    s.just_changed_8497 = false;
    s.requery_pending_8499 = false;
    s.requery_count_8488 = 0;
    s.requery_position_8352 = Vector{};
    s.requery_normal_8368 = Vector{};
    return s;
}

void Selector::reset() {
    const Packet l = launch;
    *this = new_(l);
    sampling = SelectorState::reset_sampling(launch.scalar_100);
}

rs::Result<std::vector<QueryRequest>, const char*> Selector::begin_launch(const Packet& l, Vector gravity,
                                                                          Settings settings) {
    auto prepared = candidates::prepare(l, gravity, settings);
    if (prepared.is_err()) return rs::make_err(prepared.unwrap_err());
    auto [cands, offset, correction] = prepared.unwrap();
    reset();
    launch = l;
    sampling.selection.candidate_scalar_100 = l.scalar_100;
    sampling.seed_fallback(l.position_32, l.velocity_0, gravity);
    offset_8272 = offset;
    correction_8304 = correction;
    candidates = cands;
    std::vector<QueryRequest> requests;
    requests.reserve(candidates.size());
    for (const auto& c : candidates) requests.push_back(request(c.trajectory, settings.sphere_radius));
    predictions.clear();
    for (const auto& r : requests) predictions.push_back(Prediction{QueryResult::miss(), r});
    scores.assign(requests.size(), 0.0f);
    return requests;
}

rs::Result<std::optional<QueryRequest>, const char*> Selector::begin_requery(std::uint32_t flags_2472,
                                                                             std::uint32_t flags_2488, float radius) {
    if ((flags_2472 & 0x10000000u) != 0 || (flags_2488 & 0x400000u) != 0 || !sampling.restart_allowed_8493) {
        return std::optional<QueryRequest>{};
    }
    const QueryRequest req = request(selected_candidate.trajectory, radius);
    auto v = validate_request(req);
    if (v.is_err()) return rs::make_err(v.unwrap_err());
    sampling.restart_allowed_8493 = false;
    requery_pending_8499 = true;
    // Native requery replaces slot0; it does not replace the winning candidate.
    if (!predictions.empty()) {
        predictions[0] = Prediction{QueryResult::miss(), req};
    }
    return std::optional<QueryRequest>{req};
}

QueryRequest request(Trajectory trajectory, float radius) {
    // 82D6CBxx/E844: f1 radius, f2 .25, f3 1.2, f4 duration2.
    trajectory.duration = 2.0f;
    return QueryRequest{trajectory, radius, 0.25f, 1.2f};
}

rs::Result<rs::Unit, const char*> validate_request(const QueryRequest& q) {
    const Trajectory& t = q.trajectory;
    bool bad = false;
    for (float x : {t.duration, q.radius, q.start_error, q.end_error}) {
        if (!rs::is_finite(x) || x <= 0.0f) bad = true;
    }
    // x/y/z only: the fourth lane of these guest `vector4`s is a homogeneous slot
    // the trajectory never reads.
    for (const Vector& v : {t.position, t.velocity, t.acceleration}) {
        for (int k = 0; k < 3; ++k) {
            if (!rs::is_finite(v[k])) bad = true;
        }
    }
    if (bad) return rs::make_err("Invalid native BipedAir trajectory request");
    return rs::Unit{};
}

// ------------------------------------------------------------------ adjustment.rs
void Selector::adjust_animation(std::int32_t frame, Vector animation, const Frame& axes, float radius) {
    sampling.adjustment_8336 = scale(axes[2], animation[2]);
    // 82D6DF40 is one vmaddfp after the separate forward multiply.
    correction_8304 = scale(madd(axes[1], animation[1] - radius, sampling.adjustment_8336), -1.0f);
    correction_8304[1] -= 0.08f;
    auto& s = sampling.selection;
    const Vector delta = sub(sub(s.position_6176, correction_8304),
                             s.trajectory_8144.position_at(static_cast<float>(s.landing_frame_8480) * DT));
    shift(s.trajectory_8144, static_cast<float>(frame) * DT);
    adjust(s.trajectory_8144,
           static_cast<std::int32_t>(static_cast<std::uint32_t>(s.landing_frame_8480) -
                                     static_cast<std::uint32_t>(frame)),
           delta, 0.5f);
    shift(s.trajectory_8144,
          static_cast<float>(static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(frame))) * DT);
    const Trajectory t = s.trajectory_8144;
    const Vector end = t.position_at(static_cast<float>(s.landing_frame_8480) * DT);
    if (launch.up_48[1] < 0.8f && dot(launch.up_48, flatten(t.velocity)) <= 0.0f && s.normal_6144[1] > 0.9f &&
        end[1] - t.position[1] > -0.2f) {
        // Runtime constants830BD400/330 are initialized by82F82588/826B0:
        // splat(-9.8), [0,-9.8,0,0]; not the launch gravity attribute.
        const float g = rs::from_bits(0x411ccccdu);
        const float distance = length(sub(t.position, end));
        const float square = 2.0f * distance * reciprocal(g);
        const float root =
            square == 0.0f ? 0.0f : square * physics::board_motion_output::inverse_length_squared(square, 2);
        const float duration = root * 0.66f + static_cast<float>(s.landing_frame_8480) * 0.0056666667f;
        if (duration > 0.2f) {
            const Vector acceleration{0.0f, -g, 0.0f, 0.0f};
            s.trajectory_8144.velocity = sub(scale(sub(end, t.position), reciprocal(duration)),
                                             scale(scale(acceleration, 0.5f), duration));
            s.trajectory_8144.acceleration = acceleration;
            s.trajectory_8144.duration = -1.0f;
            s.landing_frame_8480 = rs::as_i32(duration * 59.999996f);
            s.scalar_8392 = static_cast<float>(s.landing_frame_8480) * DT;
        }
    }
}

// ------------------------------------------------------------------ candidates.rs
namespace candidates {

rs::Result<std::tuple<std::vector<Candidate>, Vector, Vector>, const char*> prepare(const Packet& p, Vector gravity,
                                                                                    Settings s) {
    const std::uint64_t sum = std::uint64_t(p.kind_108) + std::uint64_t(p.kind_112);
    if (sum > 0xffffffffull) return rs::make_err("BipedAir candidate count overflow");
    const std::size_t count = static_cast<std::size_t>(sum);
    if (p.kind_108 == 0 || count > MAX_CANDIDATES || !rs::is_finite(s.height) || !rs::is_finite(s.sphere_radius) ||
        s.sphere_radius <= 0.0f) {
        return rs::make_err("Invalid authored BipedAir selector settings");
    }
    bool nonfinite = !rs::is_finite(p.scalar_96) || !rs::is_finite(p.scalar_100) || !rs::is_finite(p.scalar_104);
    // Only x/y/z are checked (the fourth lane is a homogeneous slot nothing reads).
    for (const Vector& v : {p.velocity_0, p.secondary_velocity_16, p.position_32, p.up_48, p.forward_64,
                            p.board_position_80, gravity}) {
        for (int k = 0; k < 3; ++k) {
            if (!rs::is_finite(v[k])) nonfinite = true;
        }
    }
    if (nonfinite) return rs::make_err("Nonfinite BipedAir launch packet");
    const Vector initial = scale(UP, -(s.height - s.sphere_radius));
    Vector correction{0.0f, 0.1f, 0.0f, 0.0f};
    if (p.has_board_position_116) {
        correction = add(correction, sub(p.board_position_80, add(p.position_32, initial)));
    }
    const Vector offset = add(initial, correction);
    const Vector base = sub(p.velocity_0, scale(correction, reciprocal(1.0f)));
    std::vector<Vector> velocities(count, base);
    if (count > 1) {
        const float speed = length(p.velocity_0);
        Vector axis, right;
        if (speed >= 0.001f) {
            axis = scale(p.velocity_0, reciprocal(speed));
            Vector r = cross(UP, axis);
            if (length(r) < 0.1f) {
                r = cross(axis, p.forward_64);
            }
            right = scale(r, reciprocal(length(r)));
        } else {
            axis = UP;
            right = scale(p.forward_64, -1.0f);
        }
        const Vector vertical = cross(axis, right);
        // 8252D980 is tan; Biped launch already supplies radians.
        const std::array<float, 3> tangents{rs::tan(p.scalar_96), rs::tan(p.scalar_100), rs::tan(p.scalar_104)};
        const float base_speed = length(base);
        float minimum = length(flatten(p.velocity_0));
        for (std::size_t i = 1; i < p.kind_108; ++i) {
            const float angle =
                (1.0f - (static_cast<float>(i) / static_cast<float>(p.kind_108 - 1)) * 2.0f) * rs::f32::consts::PI;
            const float sin = trigonometry::sin(angle);
            const float cos = trigonometry::cos(angle);
            const float cone = cos <= 0.0f ? tangents[1] : tangents[2];
            const Vector v = madd(vertical, cos * cone, madd(right, sin * tangents[0], base));
            velocities[i] = scale(v, base_speed * reciprocal(length(v)));
            minimum = rs::min(minimum, length(flatten(velocities[i])));
        }
        if (p.kind_112 > 0) {
            const Vector secondary = flatten(p.secondary_velocity_16);
            const float n = length(secondary);
            const Vector direction = n > 0.01f ? scale(secondary, reciprocal(n)) : normalize_or(p.forward_64, Vector{});
            const float increment = rs::min(rs::max(minimum, 1.0f), 4.0f) / static_cast<float>(p.kind_112 + 1);
            float spd = 0.0f;
            // Original822F9444=41A95811: fixed native secondary vertical seed.
            const float square = rs::from_bits(0x41a95811u);
            const float vert = square * physics::board_motion_output::inverse_length_squared(square, 2);
            for (std::size_t i = p.kind_108; i < count; ++i) {
                spd += increment;
                velocities[i] = madd(direction, spd, Vector{0.0f, vert, 0.0f, 0.0f});
            }
        }
    }
    std::vector<Candidate> out;
    out.reserve(count);
    for (const Vector& velocity : velocities) {
        Trajectory trajectory{add(p.position_32, offset), velocity, gravity, 2.0f};
        shift(trajectory, static_cast<float>(s.start_index) * DT);
        auto v = validate_request(request(trajectory, s.sphere_radius));
        if (v.is_err()) return rs::make_err(v.unwrap_err());
        Candidate c = Candidate::reset();
        c.trajectory = trajectory;
        c.start_frame_112 = s.start_index;
        out.push_back(c);
    }
    return std::make_tuple(out, offset, initial);
}

}  // namespace candidates

// ------------------------------------------------------------------ completion.rs
rs::Result<rs::Unit, const char*> Selector::observe_launch(const std::vector<QueryResult>& results) {
    if (!sampling.pending_8492 || results.size() != candidates.size()) {
        return rs::make_err("BipedAir launch completion does not match pending batch");
    }
    const std::size_t n = std::min(candidates.size(), predictions.size());
    for (std::size_t k = 0; k < n; ++k) {
        Candidate& candidate = candidates[k];
        Prediction& prediction = predictions[k];
        const QueryResult result = results[k];
        prediction.result = result;
        if (result.valid()) {
            candidate.contact_position_96 = result.contact_position;
            candidate.normal_64 = result.suggested_normal();
            prediction.result.contact_normal = candidate.normal_64;
            candidate.contact_velocity_80 =
                candidate.trajectory.velocity_at(static_cast<float>(result.contact_frame) * DT);
            candidate.landing_frame_116 = result.contact_frame;
            candidate.valid_120 = true;
        } else {
            candidate.normal_64 = UP;
            candidate.contact_velocity_80 = Vector{};
            candidate.landing_frame_116 = 120;
            candidate.valid_120 = false;
        }
    }
    return rs::Unit{};
}

rs::Result<std::size_t, const char*> Selector::select_launch(const Context& context, const Candidate& original_first,
                                                             const std::optional<ledge::Adjustment>& ledge) {
    if (!sampling.pending_8492 || candidates.empty()) {
        return rs::make_err("BipedAir select without pending launch");
    }
    if (ledge) {
        ledge->apply(candidates[0]);
        ledge_normal_8320 = candidates[0].normal_64;
        ledge_selected_8496 = true;
    }
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const Candidate c = candidates[i];
        scores[i] = 0.0f;
        if (!c.valid_120) {
            continue;
        }
        const Candidate original = i == 0 ? original_first : c;
        const Vector displacement = sub(original.contact_position_96, original.trajectory.position);
        const Vector normal = original.normal_64;
        const float time = static_cast<float>(c.landing_frame_116) * DT;
        float ledge_bonus = i == 0 && ledge_selected_8496 ? 1.0f : 0.0f;
        float time_penalty = 0.0f;
        if (time <= 0.5f) {
            ledge_bonus = 0.0f;
            time_penalty = -1.0f - (0.5f - time) * 2.0f;
        }
        const float center_bonus = i == 0 ? 1.0f : 0.0f;
        if ((predictions[i].result.surface & 0xf80u) == 0x300u) {
            predictions[i].result.contact_normal = context.up_544;
        }
        float normal_penalty = rs::min(2.2222223f * ((normal[1] - 0.45f) * 5.0f), 0.0f);
        if (dot(normal, launch.forward_64) > 0.0f) {
            normal_penalty = 0.0f;
        }
        float forward = rs::max(dot(launch.forward_64, displacement), 1.0f);
        float height = rs::max(displacement[1], 0.0f);
        if (i >= launch.kind_108) {
            if (displacement[1] < 0.4f) {
                height -= 10000.0f;
            } else {
                height *= 8.0f;
                forward = dot(launch.secondary_velocity_16, displacement);
            }
        }
        scores[i] =
            ((((0.0f + forward) + height) + normal_penalty) + center_bonus) + time_penalty + ledge_bonus;
    }
    const auto index_opt = recovered::selection::selected_index(scores);
    if (!index_opt) return rs::make_err("BipedAir empty candidate batch");
    const std::size_t index = *index_opt;
    selected_index = index;
    selected_candidate = candidates[index];
    if (sampling.commit(selected_candidate, predictions[index], offset_8272)) {
        just_changed_8497 = true;
    }
    return index;
}

rs::Result<rs::Unit, const char*> Selector::complete_requery(const Prediction& prediction) {
    if (!requery_pending_8499) {
        return rs::make_err("BipedAir requery completion without submission");
    }
    requery_pending_8499 = false;
    if (!predictions.empty()) {
        predictions[0] = prediction;
    }
    if (!prediction.result.valid()) {
        return rs::Unit{};
    }
    const QueryResult result = prediction.result;
    if (requery_count_8488 <= 0) {
        requery_position_8352 = result.contact_position;
        requery_normal_8368 = result.suggested_normal();
    } else {
        const Vector delta = sub(requery_position_8352, result.contact_position);
        if (dot(delta, delta) > 0.0001f) {
            requery_position_8352 = result.contact_position;
            requery_normal_8368 = result.suggested_normal();
            auto& s = sampling.selection;
            s.landing_frame_8480 = result.contact_frame;
            s.scalar_8392 = static_cast<float>(result.contact_frame) * DT;
            s.velocity_6160 =
                prediction.request.trajectory.velocity_at(static_cast<float>(result.contact_frame) * DT);
            s.position_6176 = result.contact_position;
            s.normal_6144 = result.suggested_normal();
        }
    }
    requery_count_8488 = static_cast<std::int32_t>(static_cast<std::uint32_t>(requery_count_8488) + 1u);
    return rs::Unit{};
}

}  // namespace s3::player::offboard::air_selector
