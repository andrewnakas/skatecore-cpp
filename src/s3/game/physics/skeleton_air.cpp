// Port of skate-game/src/physics/skeleton_air.rs.
#include "s3/game/physics/skeleton_air.hpp"

#include <algorithm>

#include "s3/core/math.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/physics/skeleton_air_frames.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/animation_input.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics::skeleton_air {

using sc::math::Basis3;
using sc::math::Vector3;
using sc::physics::board::BodyId;
using sc::point_graph::PointGraph;

rs::Result<SkeletonAir, std::string> SkeletonAir::load(const sd::Collections& data) {
    auto graph = [&](const char* name) -> rs::Result<PointGraph<8>, std::string> {
        //Original82C01900: Globals260 layout4, graphs0/80;
        //stock physics_airstates/default has80-byte PointNegGraphData8.
        S3G_TRY(auto w, data.words<20>("physics_airstates", "default", name));
        const auto words = rs::from_bits(w);
        PointGraph<8> g;
        std::copy(words.begin() + 4, words.begin() + 12, g.x.begin());
        std::copy(words.begin() + 12, words.begin() + 20, g.y.begin());
        return g;
    };
    SkeletonAir out;
    out.board_animation = sc::physics::board_animation::BoardAnimation{};
    S3G_TRY(out.settings.slow, graph("PhysToAnimSlow"));
    S3G_TRY(out.settings.fast, graph("PhysToAnimFast"));
    return out;
}

void SkeletonAir::capture_physics_error(const sc::physics::board_runtime::BoardRuntime& board,
                                        const Transform& animated_target) {
    board_animation.capture_physics_error(animated_target,
                                          matrix(board.part_transforms()[sc::physics::board::index(BodyId::Deck)]));
}

void SkeletonAir::reset_board() { board_animation.reset(); }

Transform SkeletonAir::apply_board(sc::physics::board_runtime::BoardRuntime& board, const Transform& target,
                                   bool fast) {
    const Transform effective = board_animation.apply(target, fast, settings);
    board.set_hook_transform(affine(effective));
    return effective;
}

Transform matrix(sc::physics::drive_frames::RetailAffineTransform t) {
    Transform m{};
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) m[i][j] = t.basis.columns[i][j];
    }
    m[3] = {t.translation.x, t.translation.y, t.translation.z, 0.0f};
    return m;
}

sc::physics::drive_frames::RetailAffineTransform affine(const Transform& m) {
    sc::physics::drive_frames::RetailAffineTransform t;
    for (std::size_t i = 0; i < 3; ++i) t.basis.columns[i] = {m[i][0], m[i][1], m[i][2]};
    t.translation = Vector3{m[3][0], m[3][1], m[3][2]};
    return t;
}

}  // namespace s3::game::physics::skeleton_air

namespace s3::game::physics::skeleton_input_runtime {

namespace frames = sc::physics::skeleton_air_frames;
using sc::physics::board::BodyId;

rs::Result<Transform, std::string> SkeletonInputRuntime::update_animated(
    skeleton_air::SkeletonAir& air, sc::physics::board_runtime::BoardRuntime& board, const Transform& reckoning,
    ProcessedPhysicsInput& p, SkeletonOwners& owners, sc::animation::Slice<const NativeMatrix> globals,
    const CollisionInput& collision, sc::physics::rigid_body::RetailSimulationStep simulation, bool fast_blend) {
    auto& s = owners.animated;
    //Unlike Ground82BDF530, this does NOT force initialize_heading=true.
    s.update_roots(board, reckoning, p.timestep_2604);
    const Transform target = frames::prepare_animated(s.roots, s.board_frames, drive_frames[0], p.flags_2468);
    s.board_frames.physical_board = air.apply_board(board, target, fast_blend);
    S3G_CHECK(general_update(p, owners, globals, collision, simulation));
    //82BDDC20..54 clears only next trajectory, after GeneralUpdate.
    owners.animated.finish_ground();
    owners.animation_input.fields.flags2468 = p.flags_2468;
    return target;
}

rs::Result<Transform, std::string> SkeletonInputRuntime::update_known_air(
    skeleton_air::SkeletonAir& air, sc::physics::board_runtime::BoardRuntime& board, const Transform& reckoning,
    std::array<float, 4> target_com, const sc::animation::output::physics_packet::PhysicsPosePacket& packet,
    ProcessedPhysicsInput& p, SkeletonOwners& owners, sc::animation::Slice<const NativeMatrix> globals,
    const CollisionInput& collision, sc::physics::rigid_body::RetailSimulationStep simulation) {
    auto& s = owners.animated;
    frames::AirDismountRevert revert;
    revert.requested = (packet.flags & (1u << 28)) != 0;
    //The native load zero-extends all32bits before fcfid.
    revert.frames = static_cast<std::uint32_t>(packet.air_dismount_revert_frames);
    revert.goofy = (p.flags_2476 & (1u << 2)) != 0;
    frames::update_known_air_roots(s.roots, reckoning, target_com, s.record.centre_of_mass, revert);
    const Transform target = frames::prepare_known_air(s.roots, s.board_frames, drive_frames[0], p.flags_2468);
    const Transform effective = air.apply_board(board, target, true);
    s.board_frames.physical_board = effective;
    //82C04270 uses the deck BODY COM, not Part6's mass-offset transform.
    const auto pos = board.bodies()[sc::physics::board::index(BodyId::Deck)].rates.position;
    const auto velocity = sc::physics::board_animation::target_velocity(effective[3], {pos.x, pos.y, pos.z, 0.0f},
                                                                        p.timestep_2604);
    for (auto& body : board.bodies_mut()) {
        body.rates.linear_velocity = sc::math::Vector3{velocity[0], velocity[1], velocity[2]};
    }
    frames::finish_known_air(s.roots, s.board_frames, effective);
    S3G_CHECK(general_update(p, owners, globals, collision, simulation));
    owners.animation_input.fields.flags2468 = p.flags_2468;
    //KnownAir does not reset next_trajectory at the end of this method.
    return target;
}

}  // namespace s3::game::physics::skeleton_input_runtime
