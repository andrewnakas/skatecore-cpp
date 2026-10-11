// Port of skate-game/src/physics/respawn.rs.
//! TU3 actor checkpoint manager, bound to the single-player static scene.
#include "s3/game/physics/respawn.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/ground_runtime.hpp"
#include "s3/game/physics/offboard/contact_queries.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"
#include "s3/game/physics/teleport_state.hpp"

namespace s3::game::physics::respawn {

using offboard::contact_queries::Probe;
using sc::math::Vector3;
using sc::player::respawn::Candidate;
using sc::player::respawn::Observation;
using sc::player::respawn::surface_allowed;

namespace {
std::string debug_lanes(const std::array<float, 4>& v) {
    return "[" + fmt::debug_f32(v[0]) + ", " + fmt::debug_f32(v[1]) + ", " + fmt::debug_f32(v[2]) + ", " +
           fmt::debug_f32(v[3]) + "]";
}
}  // namespace

rs::Result<Runtime, std::string> Runtime::load(const sd::Collections& data, Matrix transform, std::uint32_t stance) {
    const std::string cls = "Hash_12B64C0E804B0853";
    auto value = [&](const char* hash) { return data.float_(cls, "default", hash); };
    Runtime out{History::new_(Candidate{transform, stance, false, 0.0f}), Settings{}, 0};
    S3G_TRY(out.settings.height, value("Hash_C0526C883AF0ECCA"));
    S3G_TRY(out.settings.radius, value("Hash_CEB092E418A5B001"));
    S3G_TRY(out.settings.drop, value("Hash_8ABE098D3806D273"));
    S3G_TRY(out.settings.normal_y, value("Hash_ADD032CACF6A1C15"));
    S3G_TRY(const auto frames, data.integer(cls, "default", "Hash_10B7C3A9CC8D3721"));
    out.settings.minimum_frames = static_cast<std::uint32_t>(frames);
    out.measurements = 0;
    return out;
}

void Runtime::reset_measurements() { measurements = 0; }

Status request(const GamePhysics& physics, SkaterRuntime& skater) {
    const std::uint32_t stance = skater.animation.checkpoint_stance();
    auto& runtime = skater.respawn;
    Scene scene{physics.world, runtime.settings};
    S3G_TRY(const Candidate candidate, runtime.history.automatic(stance, scene));
    skater.animation.request_checkpoint_stance(candidate.stance);
    skater.teleport_state.reply(teleport_state::Checkpoint{candidate.transform, !candidate.offboard});
    log::info(fmt::cat("BAIL_CHECKPOINT position=", debug_lanes(candidate.transform[3]),
                       " heading=", debug_lanes(candidate.transform[2]), " stance=", candidate.stance,
                       " offboard=", candidate.offboard, " score=", candidate.score));
    return ok();
}

Status observe(const GamePhysics& physics, SkaterRuntime& skater) {
    const auto& physical = skater.player_input.physical;
    const auto& processed = skater.player_input.processed;
    // 82592A00 -> SkateboardReckoning64 ->82C01BF8: solved deck, with bit20 flip.
    auto deck = solve::deck_frame(physics.board);
    if ((processed.flags_2468 & 0x0010'0000u) != 0) flip(deck);
    deck[3][1] += 0.2f;
    auto offboard = skater.animated_skeleton.roots.animation_to_world;
    if ((processed.flags_2476 & 4u) != 0) flip(offboard);
    const auto velocity = rs::from_bits(physical.skateboard.vector_80);
    const std::uint32_t stance = skater.animation.checkpoint_stance();
    auto& runtime = skater.respawn;
    runtime.measurements = static_cast<std::int32_t>(static_cast<std::uint32_t>(runtime.measurements) + 1u);
    Observation observation{};
    observation.measurements = runtime.measurements;
    observation.root_position = skater.animated_skeleton.roots.animation_to_world[3];
    observation.com_position = rs::from_bits(physical.reckoning.vector_64);
    observation.teleport_requested = physical.state.flag_69 != 0;
    observation.physical_state = physical.state.state_16;
    observation.state_frames = skater.player_state.state_count;
    observation.ground_suppressed = (processed.grind.flags_1516 & 0x0800'0000u) != 0;
    observation.offboard_correction = skater.biped_ground.controller.state.contact.active;
    observation.ground_category = ground_runtime::active_surface(physics.riding, physics.board) & 0xffffu;
    // 82DB6EC0 publishes the same processed foot record to both fields.
    const std::uint32_t foot = (processed.left_surface_2596 >> 7) & 31u;
    observation.foot_categories = {foot, foot};
    observation.riding_transform = heading(deck, velocity);
    // 82591E30 returns immediately for offboard, before the velocity branch.
    observation.offboard_transform = offboard;
    observation.stance = stance;
    // No alternate-world/challenge controller exists in this local scene.
    observation.alternate_world = false;
    Scene scene{physics.world, runtime.settings};
    return runtime.history.observe(observation, runtime.settings.minimum_frames, scene);
}

void flip(Matrix& matrix) {
    for (std::size_t i : {std::size_t(0), std::size_t(2)}) {
        for (auto& v : matrix[i]) v = -v;
    }
}

Matrix heading(Matrix matrix, std::array<float, 4> velocity) {
    const float square = velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2];
    if (square > 0.25f) {
        const float inv = 1.0f / rs::sqrt(square);
        const std::array<float, 4> forward{velocity[0] * inv, 0.0f, velocity[2] * inv, 0.0f};
        if (forward[0] * forward[0] + forward[2] * forward[2] > 0.9f) {
            matrix[0] = {forward[2], 0.0f, -forward[0], 0.0f};
            matrix[1] = {0.0f, 1.0f, 0.0f, 0.0f};
            matrix[2] = forward;
        }
    }
    return matrix;
}

rs::Result<std::optional<Ground>, std::string> Scene::ground(const Matrix& transform) {
    auto start = transform[3];
    start[1] += 0.1f;
    auto end = start;
    end[1] -= 10.0f;
    auto probe = [](std::array<float, 4> s, std::array<float, 4> e, float radius) { return Probe{s, e, radius}; };
    // The only actor has matching identity0; canonical map groups remain active.
    S3G_TRY(const auto found, offboard::contact_queries::query(world, probe(start, end, 0.0f), 0));
    if (!found) return std::optional<Ground>{};
    const auto& hit = *found;
    const std::uint32_t category = (std::uint32_t(hit.packed_surface) >> 7) & 31u;
    if (hit.geometry.normal.y < settings.normal_y || start[1] - hit.geometry.position.y > settings.drop ||
        !surface_allowed(category)) {
        return std::optional<Ground>{};
    }
    auto bottom = start;
    bottom[1] += settings.radius;
    auto top = bottom;
    top[1] += settings.height;
    S3G_TRY(const auto blocked, offboard::contact_queries::query(world, probe(bottom, top, settings.radius), 0));
    if (blocked.has_value()) return std::optional<Ground>{};
    const auto p = hit.geometry.position;
    return std::optional<Ground>(Ground{{p.x, p.y, p.z, 0.0f}, category == 8});
}

rs::Result<bool, std::string> Scene::location(const Matrix&) {
    // 82BFBB48 returns true in normal-world mode, before provider invocation.
    return true;
}

rs::Result<bool, std::string> Scene::occupants(const Matrix&) {
    // 82BFB928 queries LivingWorldManager pedestrians/vehicles, not terrain.
    // This BoardWorld has no living-world actors. Static obstacles are checked
    // by ground's capsule; the player and board must not reject themselves.
    const auto metadata = world.query_metadata();
    if (metadata.is_err()) return rs::make_err(std::string(metadata.unwrap_err()));
    return true;
}

rs::Result<bool, std::string> Scene::edges(const Matrix& transform) {
    const auto p = transform[3];
    const sc::physics::board_world::Bounds bounds{
        Vector3::new_(p[0] - 0.3f, p[1] - 0.6f, p[2] - 0.3f),
        Vector3::new_(p[0] + 0.3f, p[1] + 0.6f, p[2] + 0.3f),
    };
    const auto metadata_r = world.query_metadata();
    if (metadata_r.is_err()) return rs::make_err(std::string(metadata_r.unwrap_err()));
    const auto* metadata = metadata_r.unwrap();
    // 82C1EAD8: authored static order, shared capacity40; no triangle diagonals.
    std::size_t taken = 0;
    for (const auto& edge : metadata->static_edges) {
        if (!edge.local_bounds.overlaps(bounds)) continue;
        if (taken == 40) break;
        ++taken;
        const std::array<float, 3> start{edge.start.x, edge.start.y, edge.start.z};
        const std::array<float, 3> end{edge.end.x, edge.end.y, edge.end.z};
        std::array<float, 3> delta{};
        for (std::size_t i = 0; i < 3; ++i) delta[i] = end[i] - start[i];
        float square = -0.0f;
        for (float v : delta) square += v * v;
        float along = -0.0f;
        for (std::size_t i = 0; i < 3; ++i) along += (p[i] - start[i]) * delta[i];
        const float fraction = square > 0.0f ? rs::clamp(along / square, 0.0f, 1.0f) : 0.0f;
        float distance = -0.0f;
        for (std::size_t i = 0; i < 3; ++i) distance += rs::powi(p[i] - start[i] - delta[i] * fraction, 2);
        if (distance < 0.09f) return false;
    }
    return true;
}

}  // namespace s3::game::physics::respawn
