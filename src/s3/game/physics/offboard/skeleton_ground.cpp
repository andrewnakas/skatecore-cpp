// Port of skate-game/src/physics/offboard/skeleton_ground.rs (+ frames.rs, state.rs)
#include "s3/game/physics/offboard/skeleton_ground.hpp"

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/physics/air_reckoning.hpp"
#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/animation_input.hpp"
#include "s3/game/physics/riding_outputs.hpp"
#include "s3/game/physics/skeleton_air.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics::skeleton_input_runtime {

using offboard::skeleton_ground::ReckoningUpdate;

rs::Result<Transform, std::string> SkeletonInputRuntime::update_biped_ground(
    skeleton_air::SkeletonAir& air, sc::physics::board_runtime::BoardRuntime& board,
    offboard::skeleton_ground::Input input, offboard::skeleton_ground::state::State& state, ProcessedPhysicsInput& p,
    SkeletonOwners& owners, sc::animation::Slice<const NativeMatrix> globals, const CollisionInput& collision,
    sc::physics::rigid_body::RetailSimulationStep simulation,
    std::function<Status(offboard::skeleton_ground::ReckoningUpdate)> finish_reckoning) {
    auto& s = owners.animated;
    const Transform target = offboard::skeleton_ground::frames::prepare_pose(
        s.roots, s.board_frames, s.record.pose[0], drive_frames[0], state.retained_board, input, p.flags_2476,
        p.flags_2484, p.flags_2468);
    if ((p.flags_2480 & 0x8000u) != 0 || (p.flags_2484 & 1u) != 0) {
        //Reuse the shared Ground/Air history and actual board anchor.
        s.board_frames.physical_board = air.apply_board(board, target, true);
    }
    S3G_CHECK(general_update(p, owners, globals, collision, simulation));
    owners.animated.finish_ground();
    const auto root = owners.animated.roots.animation_to_world;
    S3G_CHECK(finish_reckoning(ReckoningUpdate{root[1], root[2], 0.5f}));
    owners.animation_input.fields.flags2468 = p.flags_2468;
    return target;
}

}  // namespace s3::game::physics::skeleton_input_runtime

namespace s3::game::physics::offboard::skeleton_ground {

// ---- frames.rs ----
namespace frames {
using sc::physics::skeleton_animation_record::compose_affine;

Transform prepare_pose(SkeletonRootFrames& roots, SkeletonBoardFrames& board_frames, const Transform& animation_board,
                       const Transform& mapped, Transform& retained, Input input, std::uint32_t flags_2476,
                       std::uint32_t flags_2484, std::uint32_t& flags_2468) {
    //82BDEE08 consumes animation record6464+16 using the OLD world root.
    board_frames.skate_root = compose_affine(roots.animation_to_world, animation_board);
    //82BDE310 also uses the old basis, with world-space state1056 COM.
    board_frames.update_com_lift(roots.animation_to_world, input.centre_of_mass_1056, rs::from_bits(0x3e75c28fu));
    const Transform target = prepare(roots, *input.world_frame, mapped, retained, flags_2476, flags_2484, flags_2468);
    board_frames.animation_target = target;
    return target;
}

Transform prepare(SkeletonRootFrames& roots, const Transform& world_, const Transform& mapped, Transform& retained,
                  std::uint32_t flags_2476, std::uint32_t flags_2484, std::uint32_t& flags_2468) {
    Transform world = world_;
    if ((flags_2476 & 4u) != 0) {
        //830BD4A0 initializer82F825F0 splats8216DEE0=-1 across all lanes.
        for (std::size_t axis : {std::size_t(0), std::size_t(2)}) {
            for (float& value : world[axis]) value *= -1.0f;
        }
    }
    roots.initialize_heading = true;
    roots.reset_initial_alignment(world);
    if ((flags_2484 & 1u) == 0) retained = mapped;
    flags_2468 |= 0x80000u;
    return compose_affine(world, retained);
}
}  // namespace frames

// ---- state.rs ----
namespace state {
rs::Result<State, std::string> State::load(const sd::Collections& data) {
    auto graph = [&](const char* field) -> rs::Result<PointGraph<8>, std::string> {
        S3G_TRY(auto raw, data.words<16>("physics_reckoning", "default", field));
        const std::array<float, 16> words = rs::from_bits(raw);
        PointGraph<8> g{};
        for (std::size_t i = 0; i < 8; ++i) {
            g.x[i] = words[i];
            g.y[i] = words[8 + i];
        }
        return g;
    };
    State s{};
    //Skeleton ctor82BD7760..7884 initializes16016 to identity.
    s.retained_board = sc::physics::skeleton_animation_record::IDENTITY;
    S3G_TRY(auto smoothing, data.words<4>("physics_reckoning", "default", "GroundNormalSmoothing"));
    s.ground_normal_smoothing = rs::from_bits(smoothing);
    S3G_TRY(s.tilt_vs_rotation, graph("TiltVsRotGround"));
    S3G_TRY(s.tilt_vs_slope, graph("TiltVsSlopeGround"));
    return s;
}

sc::player::offboard::ground_reckoning::Output State::finish_reckoning(
    ReckoningUpdate update, riding_outputs::RidingOutputs& riding, air_reckoning::AirReckoning& air,
    const sc::player::input_phase::ProcessedPhysicsInput& processed, float physical_body_spin) const {
    namespace gr = ::s3::player::offboard::ground_reckoning;
    const auto previous = riding.reckoning.up;
    gr::Settings settings{ground_normal_smoothing, &tilt_vs_rotation, &tilt_vs_slope};
    gr::Input input{};
    input.previous_up = {previous.x, previous.y, previous.z, 0.0f};
    input.requested_up = update.up;
    input.requested_forward = update.forward;
    input.blend = update.blend;
    input.reverse_stance = (processed.flags_2468 & 0x100000u) != 0;
    input.enable_body_spin_input = true;
    input.physical_body_spin_2812 = physical_body_spin;
    return gr::update(riding.reckoning, riding.reckoning_frames, riding.body_spin, air.state, settings, input);
}
}  // namespace state

}  // namespace s3::game::physics::offboard::skeleton_ground
