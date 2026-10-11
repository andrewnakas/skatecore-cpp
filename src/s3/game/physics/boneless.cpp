// Port of skate-game/src/physics/boneless.rs.
//! Physical state602, TU3 vtable82327434. Stock no-comply/boneless/fastplant
//! animations select this state with AnimSkateboard and launch with FootJump.
#include "s3/game/physics/boneless.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/air_phase.hpp"
#include "s3/game/physics/air_trajectory.hpp"
#include "s3/game/physics/footplant/math.hpp"
#include "s3/game/physics/plant_skeleton.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"

namespace s3::game::physics::boneless {

using namespace ::s3::game::physics::footplant::math;
using sc::point_graph::PointGraph;

rs::Result<Boneless, std::string> Boneless::load(const sd::Collections& data) {
    auto graph = [&](const char* name) -> rs::Result<PointGraph<8>, std::string> {
        S3G_TRY(const auto raw, data.words<20>("Hash_CCB95A83C78B4FF9", "default", name));
        const auto words = rs::from_bits(raw);
        PointGraph<8> g;
        for (std::size_t i = 0; i < 8; ++i) {
            g.x[i] = words[4 + i];
            g.y[i] = words[12 + i];
        }
        return g;
    };
    Boneless out;
    out.toe = 15;
    out.anchor = {0.0f, 0.0f, 0.0f, 0.0f};
    out.right = false;
    S3G_TRY(out.curves[0], graph("Hash_6D781EFAF01E707D"));
    S3G_TRY(out.curves[1], graph("Hash_C9112CCD0BCB1850"));
    S3G_TRY(out.curves[2], graph("Hash_88D0CDEFA38A36D6"));
    S3G_TRY(out.curves[3], graph("Hash_9228B7F18C223F9C"));
    return out;
}

Status enter(GamePhysics& physics, SkaterRuntime& skater) {
    skater.ground_lifecycle.board_animated_290 = 1;
    auto& drive = physics.board.hook_mut().drive;
    //Globals12/912 at822F860C/822F8990; linear HardDrive, angular SoftDrive.
    drive.dynamics[0] = 0x426fffffu;
    drive.dynamics[1] = 0u;
    drive.dynamics[2] = 0x4560fffeu;
    drive.dynamics[3] = 2u;
    drive.enable_angular_soft();
    const bool right = (skater.player_input.processed.flags_2468 & (1u << 26)) != 0;
    skater.boneless.toe = right ? 19 : 15;
    skater.boneless.anchor = skater.skeleton.record.pose[skater.boneless.toe][3];
    skater.boneless.right = right;
    return ok();
}

Status update(GamePhysics& physics, SkaterRuntime& skater) {
    S3G_CHECK(plant_skeleton::advance(physics, skater, skater.boneless.anchor, skater.boneless.toe));
    plant_skeleton::hold_foot(skater, skater.boneless.right, skater.boneless.anchor, 3);
    if ((skater.player_input.processed.flags_2480 & (1u << 13)) != 0) {
        S3G_CHECK(launch(physics, skater));
    }
    return ok();
}

Status launch(GamePhysics& physics, SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    const V up = rs::from_bits(p.vectors_544_560_592_608[0]);
    const V current = rs::from_bits(p.vectors_544_560_592_608[3]);
    const V prepared = rs::from_bits(p.prepared_jump_704);
    const V horizontal = sub(prepared, scale(up, dot(prepared, up)));
    const float speed = length(horizontal);
    const auto& g = skater.boneless.curves;
    const float vertical = g[0].evaluate(up[1]) * g[1].evaluate(speed);
    const float scaled_speed = g[2].evaluate(speed) * speed;
    const V a = madd(up, vertical - dot(current, up), current);
    const V b = madd(normalize(horizontal), scaled_speed, scale(up, vertical));
    const float blend = g[3].evaluate(up[1]);
    V velocity = madd(b, blend, scale(a, 1.0f - blend));
    if (rs::abs(length(velocity) - length(current)) > 10.0f) {
        velocity = current;
    }
    const V forward = rs::from_bits(p.effective_anim_transform_192[2]);  //Processed224
    const float component = rs::max(dot(velocity, forward), 0.0f);
    if (component < 1.92f) {
        velocity = madd(forward, 1.92f - component, velocity);
    }
    const V com = rs::from_bits(p.vectors_544_560_592_608[2]);
    const V position = madd(up, dot(sub(skater.boneless.anchor, com), up) + 0.25f, com);
    S3G_TRY(auto info, air_phase::launch_info(physics, skater));
    info.start_velocity = velocity;
    info.start_position_override = position;
    info.board_position_override = position;
    info.use_position_override = true;
    info.player_jumped = true;
    S3G_TRY(const auto input, air_phase::selector_input(physics, skater));
    S3G_CHECK(skater.trajectory.launch(info, input, physics.world));
    S3G_CHECK(skater.trajectory.update(
        input, physics.world,
        air_trajectory::GrindContext::from_processed(skater.player_input.processed,
                                                     solve::deck_frame(physics.board)[3])));
    return ok();
}

}  // namespace s3::game::physics::boneless
