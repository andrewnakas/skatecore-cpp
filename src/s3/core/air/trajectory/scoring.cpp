// Port of skate-core/src/air/trajectory/scoring.rs.
#include "s3/core/air/trajectory/scoring.hpp"

#include "s3/core/air/trajectory/launch.hpp"

namespace s3::air::trajectory::scoring {

using namespace trajectory::math;
using launch::adjust_trajectory;

rs::Result<bool, std::string> score(physics::Slice<Candidate> candidates, std::uint16_t pass, bool adjusted_on_vert,
                                    SelectorInput input, const SelectorSettings& s, GrindFn grind, LineFn line) {
    bool all_miss = true;  // selector9662, consumed by PhysicsAir COM latch
    float highest_wall = -1000000.0f;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        Candidate& c = candidates[index];
        c.score = 0.0f;
        c.wall_score = 0.0f;
        c.wall_ride = false;
        c.grind = std::nullopt;
        if (!c.prediction.result.valid()) {
            continue;
        }
        all_miss = false;
        if (c.prediction.result.contact_frame < s.minimum_trajectory_frames) {
            continue;
        }
        const bool middle = index == 0 && pass == 1;
        const float middle_bonus = middle && !adjusted_on_vert ? s.score_middle_bonus : 0.0f;
        c.collision_position = c.prediction.collision_position();
        c.normal = c.prediction.result.suggested_normal();
        c.prediction.result.contact_normal = c.normal;
        c.collision_velocity = c.prediction.collision_velocity();
        const float force_dot = dot(c.normal, c.collision_velocity);
        const float sideways = length(cross(c.normal, normalize(c.collision_velocity)));
        const float scalar = s.landing_force_scalar.evaluate(rs::abs(force_dot));
        const float force_score = (force_dot * s.score_landing_force) * scalar;
        const float direction_score = (sideways * s.score_landing_direction) * scalar;

        const bool grind_middle = middle && !((input.flags_2472 & 0x20000000u) != 0 &&
                                              (input.offboard_flags_1776 & 0x04000000u) != 0 &&
                                              (input.offboard_flags_1776 & 0x08000000u) == 0);
        rs::Result<grind::GrindEvaluation, std::string> evaluation_result = grind(c.prediction, grind_middle);
        if (evaluation_result.is_err()) return rs::make_err(evaluation_result.unwrap_err());
        const grind::GrindEvaluation evaluation = evaluation_result.unwrap();
        c.grind = evaluation.target;
        float grind_score;
        if ((input.flags_2476 & 0x02000000u) != 0) {
            grind_score = 0.0f;
        } else if (evaluation.score != 0.0f) {
            grind_score = evaluation.score;
        } else if (input.grind_lock_distance > 0.5f || !middle) {
            grind_score = s.grind_penalty_vs_distance.evaluate(evaluation.penalty_input());
        } else {
            grind_score = 0.0f;
        }
        if (grind_score <= 0.0f) {
            rs::Result<float, std::string> wall = wall_score(c.prediction, grind_score < 0.0f, s, highest_wall, line);
            if (wall.is_err()) return rs::make_err(wall.unwrap_err());
            c.wall_score = wall.unwrap();
            c.wall_ride = c.wall_score > 0.0f;
            if (c.wall_score < 0.0f) {
                c.prediction.result.contact_normal = UP;
            }
        }
        float surface_score = 0.0f;
        if (grind_score <= 1.0f) {
            switch ((c.prediction.result.surface >> 7) & 31u) {
                case 6:
                    surface_score = s.surface_unrideable_score;
                    c.prediction.result.contact_normal = input.reference_up;
                    break;
                case 7:
                    surface_score = s.surface_dont_align_score;
                    c.prediction.result.contact_normal = input.reference_up;
                    break;
                case 8:
                    surface_score = s.surface_dont_align_score;
                    break;
                default:
                    break;
            }
        }
        c.normal = c.prediction.result.contact_normal;
        float transition = 0.0f;
        if (adjusted_on_vert) {
            if (input.directional_input < 0.5f && grind_score > 0.0f) {
                c.grind = std::nullopt;
                grind_score = s.grind_penalty_vs_distance.evaluate(0.0f);
            }
            const float sign = input.directional_input >= 0.5f ? 1.0f : -1.0f;
            Vector heading = input.heading_direction;
            heading[1] = 0.0f;
            const float since_apex = rs::abs(c.prediction.result.contact_time - apex_time(c.prediction));
            const float penalty = since_apex >= s.minimum_time_after_apex ? 0.0f : -500.0f;
            const float coefficient = ((1.0f - rs::abs(input.ground_normal[1])) * sign) * s.score_transition;
            transition = dot(c.normal, normalize(heading)) * coefficient + penalty;
        }
        c.score = s.landing_time_bonus.evaluate(c.prediction.result.contact_time) +
                  (((((direction_score + 1.0f) + force_score) + middle_bonus) + grind_score) + transition);
        c.score += surface_score;
    }
    // AddWallRideScores runs after ALL candidates, using the final highest Y.
    for (Candidate& c : candidates) {
        if (c.prediction.result.valid() && c.prediction.result.contact_frame >= s.minimum_trajectory_frames) {
            if (c.wall_score == 0.0f && c.prediction.collision_position()[1] > highest_wall) {
                c.wall_score = 2000.0f;
            }
            c.score += c.wall_score;
        }
    }
    return all_miss;
}

float apex_time(Prediction p) {
    const float v = p.request.trajectory.velocity[1];
    const float g = p.request.trajectory.acceleration[1];
    if (v < 0.0f || g >= 0.0f) {
        return 0.0f;
    }
    return v * reciprocal(-g);
}

rs::Result<float, std::string> wall_score(Prediction& p, bool reject_grind, const SelectorSettings& s,
                                          float& highest, LineFn& line) {
    const Vector normal = p.result.contact_normal;
    if (!(rs::abs(normal[1]) < 0.5f)) {
        return 0.0f;
    }
    const Vector start = madd(normal, s.wall_ride_test_distance, p.result.contact_position);
    const Vector end = add(start, Vector{0.0f, -10.0f, 0.0f, 0.0f});
    rs::Result<std::optional<SurfaceHit>, std::string> line_result = line(start, end, 0.0f);
    if (line_result.is_err()) return rs::make_err(line_result.unwrap_err());
    const std::optional<SurfaceHit> hit_opt = line_result.unwrap();
    if (!hit_opt) {
        return 0.0f;
    }
    const SurfaceHit hit = *hit_opt;
    if (!(dot(hit.normal, normal) < s.wall_ride_normal_dot_limit)) {
        return 0.0f;
    }
    const float height = rs::abs(hit.position[1] - start[1]);
    if (reject_grind) {
        return height * s.wall_ride_height_score - 1000.0f;
    }
    const Vector position = p.collision_position();
    Vector delta = sub(p.request.trajectory.position, position);
    float slope = delta[1];
    delta[1] = 0.0f;
    const float horizontal = length(delta);
    if (horizontal > rs::from_bits(0x3c23d70au)) {
        slope *= reciprocal(horizontal);
    }
    if (slope > s.wall_ride_angle_allow_landing) {
        return height * s.wall_ride_height_score - 1000.0f;
    }
    if (height < s.wall_ride_minimum_height) {
        return height - 1000.0f;
    }
    const float boost = s.wall_ride_boost.evaluate(height);
    if (position[1] > highest) {
        highest = position[1];
    }
    adjust_trajectory(p.request.trajectory, p.result.contact_frame, Vector{0.0f, boost, 0.0f, 0.0f},
                      s.maximum_trajectory_adjust);
    return height * s.wall_ride_height_score;
}

}  // namespace s3::air::trajectory::scoring
