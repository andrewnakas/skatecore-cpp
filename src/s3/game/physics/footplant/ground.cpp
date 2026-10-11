// Port of skate-game/src/physics/footplant/ground.rs.
//! FootPlant601, vtable82327400 and manager82D704B0/82D70E40.
#include "s3/game/physics/footplant/ground.hpp"

#include "s3/core/trigonometry.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/air_phase.hpp"
#include "s3/game/physics/air_trajectory.hpp"
#include "s3/game/physics/footplant.hpp"
#include "s3/game/physics/plant_skeleton.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"

namespace s3::game::physics::footplant {

using namespace ::s3::game::physics::footplant::math;

std::uint32_t Footplant::start(std::array<float, 4> com, std::array<float, 4> velocity) {
    perform = false;
    scalar_600 = 0.0f;
    flag_627 = true;
    const V contact_ = adjusted_contact;
    vectors_352_368[1] = contact_;
    const V offset = sub(com, contact_);
    const V radial = normalize(offset);
    const float multiplier = settings.radial_speed_scale.evaluate(length(scale(radial, dot(velocity, radial))));
    const V rotation = cross(offset, velocity);
    const float angular_speed = length(rotation);
    //830BD300 is initialized by82F82690 from821647E0.
    if (!(angular_speed > rs::from_bits(0x37800000u))) {
        return 0;
    }
    const V axis = scale(rotation, reciprocal(angular_speed));
    const float angular_rate = angular_speed * multiplier;
    //8286CD88 projects both vectors into the plane;8296EC98 signs acos.
    const V normal = scale(axis, -1.0f);
    const V up{0.0f, 1.0f, 0.0f, 0.0f};
    const V projected_up = normalize(sub(up, scale(normal, dot(up, normal))));
    float angle = length(projected_up) == 0.0f
                      ? 0.0f
                      : sc::trigonometry::acos(rs::clamp(dot(radial, projected_up), -1.0f, 1.0f));
    if (dot(cross(radial, projected_up), normal) < 0.0f) {
        angle = -angle;
    }
    const float min_end = angle + angular_rate * settings.min_duration;
    const float max_end = angle + angular_rate * settings.max_duration;
    const float end_angle = rs::min(rs::max(min_end, settings.end_angle * rs::from_bits(0x3c8efa35u)), max_end);
    const float delta = end_angle - angle;
    scalar_596 = delta / angular_rate;
    //Quaternion(axis,delta) applied in the native two-cross-product order.
    const auto [sin, cos] = sc::trigonometry::sin_cos(delta * 0.5f);
    const V q = scale(axis, sin);
    const V end_radial = madd(cross(q, madd(radial, cos, cross(q, radial))), 2.0f, radial);
    const V end = madd(end_radial, settings.end_leg_length, contact_);
    const V middle = normalize(add(end_radial, radial));
    const V release = madd(sub(velocity, scale(middle, dot(velocity, middle))), multiplier,
                           scale(middle, settings.release_outward_speed));
    scalar_612 = length(release);
    vectors_352_368[0] = scale(release, reciprocal(scalar_612));
    curve = {
        com,
        madd(scale(velocity, multiplier), scalar_596 * settings.start_handle, com),
        sub(end, scale(release, scalar_596 * settings.end_handle)),
        end,
    };
    return std::uint32_t(4) - static_cast<std::uint32_t>(rs::as_i32(scalar_596 * rs::from_bits(0xc26fffffu)));
}

void Footplant::adjust(float t, std::array<float, 2> input) {
    const float amount = std::fma(1.0f - t, 0.02f, t * 0.01f);
    V movement{input[0] * amount, 0.0f, input[1] * amount, 0.0f};
    const float projection = dot(movement, normalize(current_up));
    if (projection > 0.0f) {
        movement = sub(movement, scale(current_up, projection));
    }
    const V previous = curve[3];
    curve[3] = add(previous, movement);
    const V direction = normalize(sub(curve[3], curve[2]));
    curve[2] = sub(curve[3], scale(direction, length(sub(curve[2], previous))));
    vectors_352_368[0] = direction;
}

}  // namespace s3::game::physics::footplant

namespace s3::game::physics::footplant::ground {

using namespace ::s3::game::physics::footplant::math;

Status enter(GamePhysics& physics, SkaterRuntime& skater) {
    physics.board.hook_mut().drive.enable_angular_only(skater.ground_lifecycle.board_animated_290);
    //82D91330 sets the foot/toe volume group and clears their disable counters.
    for (const std::size_t part : {15, 16, 19, 20}) {
        skater.skeleton_collision.parts[part].volume_group = 4;
        skater.skeleton_collision.parts[part].enabled = false;
        skater.skeleton_collision.disable_count[part] = 0;
    }
    const auto& p = skater.player_input.processed;
    const std::uint32_t frames = skater.footplant.start(rs::from_bits(p.vectors_544_560_592_608[2]),
                                                        rs::from_bits(p.vectors_544_560_592_608[3]));
    if (frames != 0) {
        //The native Start only disables collisions; the first Update sets IK.
        const bool right = skater.footplant.selected_toe == std::optional<std::size_t>(19);
        const std::array<std::size_t, 3> parts =
            right ? std::array<std::size_t, 3>{19, 20, 21} : std::array<std::size_t, 3>{15, 16, 17};
        for (const std::size_t part : parts) {
            skater.skeleton_collision.pending_reenable = true;
            skater.skeleton_collision.disable_count[part] = frames;
            skater.skeleton_collision.parts[part].enabled = false;
        }
    }
    return ok();
}

Status update(GamePhysics& physics, SkaterRuntime& skater) {
    auto& fp = skater.footplant;
    fp.scalar_600 += STEP;
    const float t = fp.scalar_600 / fp.scalar_596;
    fp.adjust(t, skater.animation_input.extra.body_adjust);
    const float u = 1.0f - t;
    const V position =
        madd(fp.curve[3], t * t * t,
             madd(fp.curve[2], 3.0f * t * t * u, madd(fp.curve[0], u * u * u, scale(fp.curve[1], 3.0f * t * u * u))));
    const bool launched = fp.scalar_600 > fp.scalar_596;
    if (launched) {
        fp.flag_627 = false;
    }
    const V anchor = fp.vectors_352_368[1];
    const V velocity = scale(fp.vectors_352_368[0], fp.scalar_612);
    const bool right = fp.selected_toe != std::optional<std::size_t>(15);
    plant_skeleton::hold_foot(skater, right, anchor, 0);
    S3G_CHECK(plant_skeleton::advance(physics, skater, position, std::nullopt));
    if (launched) {
        S3G_TRY(auto info, air_phase::launch_info(physics, skater));
        info.start_velocity = velocity;
        info.start_position_override = add(anchor, V{0.0f, 0.2f, 0.0f, 0.0f});
        info.board_position_override = add(info.start_position_override, V{0.0f, -0.1f, 0.0f, 0.0f});
        info.use_position_override = true;
        info.player_jumped = true;
        S3G_TRY(const auto input, air_phase::selector_input(physics, skater));
        S3G_CHECK(skater.trajectory.launch(info, input, physics.world));
        S3G_CHECK(skater.trajectory.update(
            input, physics.world,
            air_trajectory::GrindContext::from_processed(skater.player_input.processed,
                                                         solve::deck_frame(physics.board)[3])));
    }
    return ok();
}

void post_physics(SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    const V down = scale(rs::from_bits(p.vectors_544_560_592_608[0]), -1.0f);
    const V velocity = rs::from_bits(p.vectors_544_560_592_608[3]);
    const float descending = dot(velocity, down);
    const float horizontal = length(sub(velocity, scale(down, descending)));
    if (descending > skater.footplant.settings.max_descending_speed) {
        skater.wipeout.state.request(6, descending);
    }
    if (horizontal > skater.footplant.settings.max_horizontal_speed) {
        skater.wipeout.state.request(23, 0.0f);
    }
}

}  // namespace s3::game::physics::footplant::ground
