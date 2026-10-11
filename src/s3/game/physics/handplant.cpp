// Port of skate-game/src/physics/handplant.rs
//! TU3 HandPlantManager82D61040 and physical state600 (823273CC).
#include "s3/game/physics/handplant.hpp"

#include <sstream>

#include "s3/core/air/trajectory/grind_surface.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/footplant/math.hpp"
#include "s3/game/physics/plant_skeleton.hpp"
#include "s3/game/physics/player_input/grind/world.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::handplant {

using namespace ::s3::game::physics::footplant::math;

namespace {
std::string vfmt(const V& v) {
    return fmt::cat("[", fmt::debug_f32(v[0]), ", ", fmt::debug_f32(v[1]), ", ", fmt::debug_f32(v[2]), ", ",
                    fmt::debug_f32(v[3]), "]");
}
}  // namespace

rs::Result<Handplant, std::string> Handplant::load(const sd::Collections& data) {
    S3G_TRY(auto settings, Settings::load(data));
    Handplant h{};
    h.settings = settings;
    return h;
}

std::array<float, 3> Handplant::animation_thresholds() const { return settings.animation; }

void Handplant::reset() {
    flags &= ~0xb0000000u;
    phase = rs::f32::MAX;
    anchor = V{};
    pending.reset();
    candidate.reset();
    elapsed = -1.0f;
    warped = -1.0f;
    apex = -1.0f;
    estimated_phase = -1.0f;
    out_duration = 0.0f;
    direction = V{};
    continuation = true;
}

void Handplant::full_reset() {
    reset();
    reset_ik();
    direction_hint = 0;
    direction_count = 0;
}

void Handplant::reset_ik() {
    ik_blend = 0.0f;
    ik_distance = -1.0f;
    ik_released = false;
    ik_latched = false;
}

void Handplant::estimate_apex() {
    //82D63280 averages four authored time-warp samples.
    const float offset = warped - apex;
    const float stride = rs::abs(offset * rs::from_bits(0x3eaaaaabu));  //Globals440
    float sum = -0.0f;
    for (int i = 0; i < 4; ++i) {
        sum += 1.0f + settings.time_warp.evaluate(offset + static_cast<float>(i) * stride);
    }
    const float mean = sum * 0.25f;
    if (mean != 0.0f) {
        estimated_phase = (-1.0f / mean) * offset;
    }
    phase = estimated_phase;
}

float apex_time(Trajectory t) {
    if (t.velocity[1] < 0.0f || t.acceleration[1] >= 0.0f) {
        return 0.0f;
    }
    return t.velocity[1] * reciprocal(-t.acceleration[1]);
}

///Ground82D37F38 consumes the candidate query before its shared skeleton update.
Status ground_update(GamePhysics& physics, SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    if ((p.flags_2476 & (1u << 22)) == 0) {
        skater.handplant.full_reset();
        return ok();
    }
    if (skater.handplant.pending) {
        const auto [candidate, com, velocity, normal, heading] = *skater.handplant.pending;
        skater.handplant.pending.reset();
        namespace grind_surface = ::s3::air::trajectory::grind_surface;
        grind_surface::InvestigationInput query{};
        query.start = candidate.edge.start;
        query.end = candidate.edge.end;
        query.reference = candidate.point;
        query.optional_probe = std::nullopt;
        query.deck_center_to_truck = skater.handplant.settings.truck_distance;
        S3G_TRY(const auto surface,
                (grind_surface::investigate<std::string>(
                    query, [&](std::size_t index, grind_surface::Probe probe) {
                        return player_input::grind::world::surface_probe(
                            physics.world, {p.actor_query_2948, p.actor_query_2952}, index, probe);
                    })));
        log::info(fmt::cat("HANDPLANT_SURFACE tick=", physics.ticks, " kind=", static_cast<std::uint32_t>(surface.kind),
                           " point=", vfmt(candidate.point)));
        if (grind_surface::prepare(query).has_value() && surface.kind != grind_surface::GeometryType::Impossible) {
            skater.handplant.launch(candidate, com, velocity, normal, heading,
                                    physics.riding.reckoning_frames.heading);
        }
    }
    if ((skater.handplant.flags & 0x80000000u) != 0) {
        ik::update(skater, true);
    }
    return ok();
}

/// Capture the solved pose before the bail check, rather than comparing this
/// tick's targets against last tick's bodies in HANDPLANT_POSE.
void trace_solved(const GamePhysics& physics, const SkaterRuntime& skater) {
    if (physics.ticks % 6 != 0 && skater.collision_maximum_error.value_or(0.0f) < 0.15f) {
        return;
    }
    if (!log::enabled()) return;  // trace-only: avoid building the message when logging is off
    const auto& root = skater.animated_skeleton.roots.animation_to_world;
    const auto actual = skater.skeleton.part_transforms();
    const std::array<std::size_t, 8> parts{3, 4, 5, 6, 7, 8, 9, 10};
    std::ostringstream os;
    os << "HANDPLANT_SOLVED tick=" << physics.ticks << " phase=" << fmt::display_f32(skater.handplant.phase)
       << " parts=[3, 4, 5, 6, 7, 8, 9, 10] authored=[";
    for (std::size_t k = 0; k < parts.size(); ++k)
        os << (k ? ", " : "") << vfmt(point(root, skater.animated_skeleton.record.pose[parts[k]][3]));
    os << "] targets=[";
    for (std::size_t k = 0; k < parts.size(); ++k)
        os << (k ? ", " : "") << vfmt(point(root, skater.skeleton_input.drive_frames[parts[k]][3]));
    os << "] solved=[";
    for (std::size_t k = 0; k < parts.size(); ++k) os << (k ? ", " : "") << vfmt(actual[parts[k]][3]);
    os << "] partial=" << (skater.skeleton_collision.partial_ragdoll ? "true" : "false")
       << " collision_weight=" << fmt::display_f32(skater.collision_feedback.drive_weight)
       << " anchor=" << vfmt(skater.handplant.anchor);
    log::info(os.str());
}

///Ground82D38430 submits the candidate before82D37F38 consumes its investigation.
void ground_query(const GamePhysics& physics, SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    if ((p.flags_2476 & (1u << 22)) == 0) {
        skater.handplant.full_reset();
        return;
    }
    auto& h = skater.handplant;
    const V com = rs::from_bits(p.vectors_544_560_592_608[2]);
    const V velocity = rs::from_bits(p.vectors_400_416[0]);
    const V normal = rs::from_bits(p.vectors_464_480_496_512_528[0]);
    const V heading = rs::from_bits(p.vectors_544_560_592_608[1]);
    if ((p.flags_2480 & (1u << 29)) != 0 || (p.flags_2480 & (1u << 28)) == 0) {
        h.direction_hint = 0;
        h.direction_count = 0;
    } else if (normal[1] < 0.95f) {
        if (h.direction_hint == 0) {
            h.direction_hint = h.candidate ? h.candidate->side : 0;
        }
        const std::int32_t side = dot(velocity, normalize(cross(UP, normal))) > 0.0f ? 1 : 2;
        if (h.direction_hint == side) {
            h.direction_count = 0;
        } else {
            h.direction_count += 1;
            if (h.direction_count > h.settings.direction_frames) {
                h.direction_hint = 0;
            }
        }
    } else {
        h.direction_hint = 0;
        h.direction_count = 0;
    }
    const auto& primitives = physics.grind_world->primitives();
    const auto candidate = contact::select(h.settings, com, velocity, normal, h.direction_hint, primitives);
    if (candidate.has_value() || physics.ticks % 30 == 0) {
        log::info(fmt::cat("HANDPLANT_QUERY tick=", physics.ticks, " speed=", length(velocity), " vy=", velocity[1],
                           " normal=", vfmt(normal), " minimum_speed=", h.settings.minimum_speed,
                           " minimum_slope=", h.settings.minimum_slope, " edges=", primitives.size(),
                           " candidate=", candidate ? fmt::cat("Some(", vfmt(candidate->point), ")") : "None",
                           " prior_flags=", h.flags, " phase=", h.phase, " processed=", p.flags_2476, "/",
                           p.flags_2480));
    }
    //82D61268 clears the active output when submission begins.
    const V old_point = h.candidate ? h.candidate->point : V{};
    const std::int32_t hint = h.direction_hint;
    const std::int32_t count = h.direction_count;
    h.reset();
    h.previous_candidate_point = old_point;
    h.direction_hint = hint;
    h.direction_count = count;
    h.candidate = candidate;
    if (candidate) {
        h.pending = std::make_tuple(*candidate, com, velocity, normal, heading);
    } else {
        h.pending.reset();
    }
}

Status enter(GamePhysics& physics, SkaterRuntime& skater) {
    physics.board.hook_mut().drive.enable_angular_only(skater.ground_lifecycle.board_animated_290);
    const auto& p = skater.player_input.processed;
    auto& h = skater.handplant;
    h.entry = Trajectory{rs::from_bits(p.vectors_544_560_592_608[2]), rs::from_bits(p.vectors_544_560_592_608[3]),
                         GRAVITY, -1.0f};
    h.warped = 0.0f;
    h.elapsed = 0.0f;
    h.continuation = true;
    S3G_CHECK(h.select_outgoing(physics.world));
    return ok();
}

Status update(GamePhysics& physics, SkaterRuntime& skater) {
    //82D4C3D8 ->82D91298: keep planted hands out of collision response.
    skater.skeleton_collision.disable_handplant_contacts(2);
    auto& h = skater.handplant;
    h.warped += (1.0f + h.settings.time_warp.evaluate(h.warped - h.apex)) * STEP;
    h.estimate_apex();
    h.elapsed += STEP;
    std::array<std::array<V, 4>, 24> pose{};
    for (std::size_t i = 0; i < pose.size(); ++i) {
        pose[i] = ::s3::physics::skeleton_animation_record::compose_affine(
            skater.animated_skeleton.roots.animation_to_world, skater.animated_skeleton.record.pose[i]);
    }
    const auto [com, up, heading] =
        h.values(pose, rs::from_bits(skater.player_input.processed.vectors_544_560_592_608[2]));
    skater.air_reckoning.update_plant(physics.riding, skater.player_input.processed, up, heading);
    ik::update(skater, false);
    S3G_CHECK(plant_skeleton::advance(physics, skater, com, std::nullopt));
    if ((physics.ticks % 6 == 0 || skater.handplant.elapsed <= STEP * 1.5f) && log::enabled()) {
        const auto& root = skater.animated_skeleton.roots.animation_to_world;
        const std::array<std::size_t, 6> parts{23, 1, 3, 7, 15, 19};  //hips, head, hands, toes
        const auto actual = skater.skeleton.part_transforms();
        std::ostringstream os;
        os << "HANDPLANT_POSE tick=" << physics.ticks << " elapsed=" << fmt::display_f32(skater.handplant.elapsed)
           << " phase=" << fmt::display_f32(skater.handplant.phase) << " com=" << vfmt(com) << " up=" << vfmt(up)
           << " heading=" << vfmt(heading) << " targets=[";
        for (std::size_t k = 0; k < parts.size(); ++k)
            os << (k ? ", " : "") << vfmt(point(root, skater.skeleton_input.drive_frames[parts[k]][3]));
        os << "] actual=[";
        for (std::size_t k = 0; k < parts.size(); ++k) os << (k ? ", " : "") << vfmt(actual[parts[k]][3]);
        os << "] hands=[";
        for (std::size_t k = 2; k < 4; ++k)
            os << (k > 2 ? ", " : "") << "(" << vfmt(skater.foot_ik.state.external_targets[k].world_position) << ", "
               << fmt::debug_f32(skater.foot_ik.state.limbs[k].target_blend) << ")";
        os << "] force_mode=" << skater.skeleton_input.force_mode
           << " flags=" << skater.player_input.processed.flags_2468 << "/"
           << skater.player_input.processed.flags_2472 << "/" << skater.player_input.processed.flags_2476;
        log::info(os.str());
    }
    return ok();
}

}  // namespace s3::game::physics::handplant
