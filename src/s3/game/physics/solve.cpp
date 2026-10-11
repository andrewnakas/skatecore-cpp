// Port of skate-game/src/physics/solve.rs.
#include "s3/game/physics/solve.hpp"

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/board_step.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/game/glam_ext.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/colliders.hpp"
#include "s3/game/physics/network.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/skeleton_colliders.hpp"

namespace s3::game::physics::solve {

using sc::physics::board::BodyId;
using sc::physics::board_step::ATTACHED_REACTION_BASE;
using sc::physics::board_step::AttachedStep;
using sc::physics::board_step::BoardCollision;
using sc::physics::skeleton_animation_record::AnimationPartTransform;
using sc::physics::skeleton_body::PART_COUNT;

Status advance(GamePhysics& physics, SkaterRuntime& skater, std::array<float, 2> truck_targets) {
    const auto before = diagnostics::snapshot(physics, skater);
    {
        auto valid = diagnostics::validate(before, "before shared solve");
        if (valid.is_err()) {
            return rs::make_err(fmt::cat(valid.unwrap_err(), "; (com/lifted/root/biped diagnostics omitted)"));
        }
    }
    auto board_volumes = colliders::world_volumes(physics.board, physics.settings);
    {
        std::vector<sc::physics::board_world::BoardWorldVolume> kept;
        for (const auto& volume : board_volumes) {
            if (skater.board_possession_live.volume_enabled(volume.body)) kept.push_back(volume);
        }
        board_volumes = std::move(kept);
    }
    S3G_TRY(auto skeleton_volumes, skeleton_colliders::enabled_volumes(skater.skeleton, skater.skeleton_collision));
    // Each native assembly has its own query record and retention buffer.
    // Skeleton82BE5094 passes false to82768728: its edge threshold is -1,
    // whereas the board requests .999. GroundPipeline supplies the remaining
    // shared values. Do not let the second query overwrite the first's rows.
    std::vector<BoardCollision> contacts;
    {
        const auto rows = physics.world.query_primitives(board_volumes, physics.query, physics.retention);
        contacts.assign(rows.begin(), rows.end());
    }
    auto skeleton_query = physics.query;
    skeleton_query.edge_cos_bend_normal_threshold = -1.0f;
    auto skeleton_world_volumes = skeleton_volumes;
    skeleton_colliders::retain_world_volumes(skeleton_world_volumes, skater.skeleton_collision);
    {
        const auto rows = physics.world.query_primitives(skeleton_world_volumes, skeleton_query, physics.retention);
        contacts.insert(contacts.end(), rows.begin(), rows.end());
    }
    S3G_CHECK(assembly_contacts::append(contacts, board_volumes, skeleton_volumes, physics.board.collision_group(),
                                        skater.skeleton_collision));
    // Remote actors use separate reaction indices after skeleton and targets.
    // Never apply the single-skater self-culling bitmap to another player.
    const std::size_t before_remote = contacts.size();
    auto remote = [&](const sc::physics::board_world::BoardWorldVolume& a) {
        for (const auto& b : physics.network_proxies.volumes) {
            const auto [ac, ar] = network::bounds(a.primitive);
            const auto [bc, br] = network::bounds(b.primitive);
            if (glam::distance_squared(ac, bc) <= rs::powi(ar + br + 0.05f, 2)) {
                assembly_contacts::append_pair(contacts, a, b);
            }
        }
    };
    for (const auto& a : board_volumes) remote(a);
    for (const auto& a : skeleton_volumes) remote(a);
    physics.network_contacts = contacts.size() - before_remote;
    physics.contact_count = contacts.size();
    const float dt = physics.settings.step.simulation.time_step;
    auto joints = skater.skeleton_joints.build(skater.skeleton.bodies(), ATTACHED_REACTION_BASE, dt);
    auto drives = skater.skeleton_drives.build(skater.skeleton.bodies(), ATTACHED_REACTION_BASE,
                                               ATTACHED_REACTION_BASE + PART_COUNT, dt);
    const std::size_t skeleton_drive_count = drives.rows.size();
    //82D74FD8: persistent hand drives share the deck and skeleton reactions.
    skater.board_possession.append_drives(
        physics.board.bodies()[sc::physics::board::index(BodyId::Deck)],
        {skater.skeleton.bodies()[3], skater.skeleton.bodies()[7]}, sc::physics::board::index(BodyId::Deck),
        {ATTACHED_REACTION_BASE + 3, ATTACHED_REACTION_BASE + 7}, dt, drives.rows);
    AttachedStep attached{};
    for (auto& body : skater.skeleton.bodies_mut()) attached.bodies.push_back(&body);
    for (auto& body : skater.skeleton_drives.targets.bodies) attached.bodies.push_back(&body);
    for (auto& body : physics.network_proxies.bodies) attached.bodies.push_back(&body);
    attached.joints = joints;
    attached.drives = drives.rows;
    physics.board.advance_attached(contacts, truck_targets, physics.settings.step, std::move(attached));
    {
        auto valid = diagnostics::validate(diagnostics::snapshot(physics, skater), "after shared solve");
        if (valid.is_err()) return rs::make_err(valid.unwrap_err());
    }
    skater.skeleton.publish_physical_record(deck_frame(physics.board));
    // These solved rows are consumed by the actual collision/drive feedback
    // phase; keep their identity and impulses after the shared solve.
    // Possession drives have no skeleton spy identity. They participate in the
    // same solve above, but must not enter the skeleton-only feedback batch.
    drives.rows.resize(std::min(drives.rows.size(), skeleton_drive_count));
    skater.solved_drives = std::move(drives);
    return ok();
}

AnimationPartTransform deck_frame(const sc::physics::board_runtime::BoardRuntime& board) {
    const auto deck = board.part_transforms()[sc::physics::board::index(BodyId::Deck)];
    auto frame = sc::physics::skeleton_animation_record::IDENTITY;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        for (std::size_t k = 0; k < 3; ++k) frame[axis][k] = deck.basis.columns[axis][k];
    }
    frame[3] = {deck.translation.x, deck.translation.y, deck.translation.z, 0.0f};
    return frame;
}

}  // namespace s3::game::physics::solve
