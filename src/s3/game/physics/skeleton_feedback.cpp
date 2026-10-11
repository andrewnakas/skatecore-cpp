// Port of skate-game/src/physics/skeleton_feedback.rs.
//! Post-solver contact reports and physical/animation errors for the live skater.
//! Engine report storage is ours; spy arithmetic and report order are native.
#include "s3/game/physics/skeleton_feedback.hpp"

#include "s3/core/physics/board_step.hpp"
#include "s3/core/physics/contact_feedback.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"

namespace s3::game::physics::skeleton_feedback {

using sc::physics::assembly::BodySnapshot;
using sc::physics::board_step::CollisionBody;
using sc::physics::skeleton_animation_record::AnimationPartTransform;
using sc::physics::skeleton_board_frames::SkeletonBoardFrames;
using sc::physics::skeleton_body::SkeletonCollisionInput;
using sc::physics::skeleton_body::SkeletonContactBody;
using sc::physics::skeleton_body::SkeletonContactReport;
using sc::physics::skeleton_root::SkeletonRootFrames;

void publish(const GamePhysics& physics, SkaterRuntime& skater, bool request_partial_ragdoll) {
    const auto reports = collect(physics, skater);
    const auto& p = skater.player_input.processed;
    const auto deck = solve::deck_frame(physics.board);
    skater.skeleton_output.correction.observe_board(deck[3],
                                                    skater.animated_skeleton.roots.predicted_board_position);
    const std::uint32_t state = p.state_2508;
    const bool offboard = p.category_2512 == 500 && !(state == 501 || state == 503);
    auto point = rs::from_bits(p.vectors_464_480_496_512_528[(state == 601 || state == 602) ? 1 : 2]);
    // Both82BD8280 and82BE2538 read Skeleton16424 ->Reckoning1216.
    const auto n = physics.riding.reckoning.ground_normal;
    std::array<float, 4> normal{n.x, n.y, n.z, 0.0f};
    if (offboard && (p.flags_2480 & 2u) != 0) {
        point = rs::from_bits(p.vectors_880_896_912_928_944[0]);
        normal = rs::from_bits(p.vectors_880_896_912_928_944[1]);
    }
    const auto frames = skater.skeleton.part_transforms();
    skater.collision_feedback.update(
        SkeletonCollisionInput{
            physics.settings.step.simulation.time_step,
            point,
            normal,
            state == 201 ? skater.skeleton.record.velocities[23] : skater.skeleton_input.deck_velocity,
            skater.animated_skeleton.board_frames.com_velocity,
            skater.skeleton_collision.is_ragdoll,
            (p.flags_2472 & 0x8000u) != 0,
            (p.flags_2472 & 0x1000'0000u) != 0,
            offboard,
            state == 501 || state == 503,
            p.category_2512 == 600,
            request_partial_ragdoll,
            skater.skeleton.record,
            skater.skeleton.definition.animation_masses.part_weights,
            frames,
        },
        reports);
    skater.skeleton_collision.finish_contact_frame();
    skater.pose_errors.targets = skater.skeleton_input.extra_target_positions;
    skater.pose_errors.update(skater.skeleton.record, skater.animated_skeleton.roots.animation_to_world,
                              skater.skeleton_input.drive_frames);
    const auto response = skater.skeleton_collision.partial_ragdoll
                              ? skater.pose_errors.partial_response()
                              : skater.pose_errors.normal_response(skater.collision_feedback,
                                                                   std::array<float, 4>{n.x, n.y, n.z, 0.0f});
    skater.collision_pose_error = response.impulse;
    skater.collision_extra_errors = response.extra;
    skater.collision_maximum_error = response.maximum_error;
    publish_board_observations(skater.animated_skeleton.board_frames, skater.animated_skeleton.roots, deck,
                               skater.skeleton.record.centre_of_mass, physics.settings.step.simulation.time_step,
                               p.flags_2472);
}

void publish_board_observations(SkeletonBoardFrames& frames, const SkeletonRootFrames& roots,
                                const AnimationPartTransform& deck, std::array<float, 4> physical_com, float dt,
                                std::uint32_t flags_2472) {
    // 82BD9F3C..9F70 transforms retained16144 into16208;9F90 uses the
    // current deck for16240. Only later82BDA024 replaces16144 with physical
    // COM10928. Contact/error processing above does not consume these locals.
    frames.publish_local_observations(roots, deck);
    frames.publish_centre_of_mass(physical_com, dt, flags_2472);
}

std::vector<SkeletonContactReport> collect(const GamePhysics& physics, const SkaterRuntime& skater) {
    const auto rows = physics.board.solved_contacts();
    std::vector<std::uint32_t> storage;
    storage.reserve(rows.size() * 64);
    for (const auto& row : rows) storage.insert(storage.end(), row.words.begin(), row.words.end());
    std::uint32_t count = 0;
    sc::physics::contact_feedback::spy_contact_jacobians(
        storage, std::uint32_t(rows.size()), count, physics.settings.step.simulation.frequency,
        [&](std::uint32_t id) -> std::array<std::uint32_t, 4> {
            const BodySnapshot* body = resolve(id, physics, skater);
            if (body == nullptr) return {0, 0, 0, 0};
            const auto p = body->rates.position;
            return {rs::to_bits(p.x), rs::to_bits(p.y), rs::to_bits(p.z), 0};
        });
    std::vector<SkeletonContactReport> reports;
    reports.reserve(16);
    const std::size_t spy_words = std::size_t(count) * 28;
    if (spy_words > storage.size()) S3_PANIC("range end index out of range for slice");
    for (std::size_t base = 0; base + 28 <= spy_words; base += 28) {
        const std::uint32_t* spy = storage.data() + base;
        const CollisionBody a = CollisionBody::from_contact_id(spy[24]);
        const CollisionBody b = CollisionBody::from_contact_id(spy[25]);
        // 82768418 rejects the same logical Body and equal nonnull Body+32
        // owners. Board82C067F4 and Skeleton82BE4620 both store this actor.
        // Their eligible cross contacts still participate in the shared solve.
        const std::size_t remote_base = skater.skeleton.bodies().size() + skater.skeleton_drives.targets.bodies.size();
        auto external = [&](CollisionBody body) {
            return body.is_static_world() || (body.is_attached() && body.attached >= remote_base);
        };
        if (!external(a) && !external(b)) continue;
        struct Side {
            CollisionBody own;
            CollisionBody other;
            bool side_a;
        };
        for (const Side s : {Side{a, b, true}, Side{b, a, false}}) {
            if (!s.own.is_attached()) continue;
            const std::size_t part = s.own.attached;
            if (part >= 24 || reports.size() == 16) continue;
            auto vector = [&](std::size_t offset) {
                std::array<float, 4> v{};
                for (std::size_t i = 0; i < 4; ++i) v[i] = rs::from_bits(spy[offset + i]);
                return v;
            };
            SkeletonContactReport r;
            r.part = part;
            const float sign = s.side_a ? 1.0f : -1.0f;
            r.normal = vector(0);
            for (auto& v : r.normal) v = v * sign;
            r.point = vector(12);
            r.tag = s.side_a ? (spy[26] & 0xffffu) : (spy[26] >> 16);
            switch (s.other.kind) {
                case CollisionBody::Kind::StaticWorld: r.other_group = 0; break;
                case CollisionBody::Kind::Board: r.other_group = physics.board.collision_group(); break;
                case CollisionBody::Kind::Attached: r.other_group = skater.skeleton_collision.assembly_group; break;
            }
            r.other_entity = std::nullopt;
            r.body_a = contact_body(resolve(spy[24], physics, skater));
            r.body_b = contact_body(resolve(spy[25], physics, skater));
            r.side_a = s.side_a;
            r.solved_vector = vector(20);
            reports.push_back(r);
        }
    }
    return reports;
}

const BodySnapshot* resolve(std::uint32_t id, const GamePhysics& physics, const SkaterRuntime& skater) {
    const CollisionBody body = CollisionBody::from_contact_id(id);
    switch (body.kind) {
        case CollisionBody::Kind::StaticWorld: return nullptr;
        case CollisionBody::Kind::Board: return &physics.board.bodies()[sc::physics::board::index(body.board)];
        case CollisionBody::Kind::Attached: {
            std::size_t part = body.attached;
            const auto& skeleton = skater.skeleton.bodies();
            if (part < skeleton.size()) return &skeleton[part];
            part -= skeleton.size();
            const auto& targets = skater.skeleton_drives.targets.bodies;
            if (part < targets.size()) return &targets[part];
            part -= targets.size();
            const auto& remote = physics.network_proxies.bodies;
            if (part < remote.size()) return &remote[part];
            return nullptr;
        }
    }
    return nullptr;
}

SkeletonContactBody contact_body(const BodySnapshot* body) {
    if (body != nullptr) {
        const auto v = body->rates.linear_velocity;
        return SkeletonContactBody{body->state_flags, body->inertia.inverse_mass, {v.x, v.y, v.z, 0.0f}};
    }
    return SkeletonContactBody{1, 0.0f, {0.0f, 0.0f, 0.0f, 0.0f}};
}

}  // namespace s3::game::physics::skeleton_feedback
