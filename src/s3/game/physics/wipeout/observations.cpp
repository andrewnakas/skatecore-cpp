// Port of skate-game/src/physics/wipeout/observations.rs
//! Borrow the actual completed collision and physical owners; no default contacts.
#include "s3/game/physics/wipeout/observations.hpp"

namespace s3::game::physics::wipeout::observations {

namespace {
std::array<float, 4> from_bits4(const std::array<std::uint32_t, 4>& v) { return rs::from_bits(v); }
}  // namespace

rs::Result<Frame, std::string> Observations::frame() const {
    const auto& p = processed;
    const auto& c = collision;
    const auto& b = board;
    Frame f;
    f.flags_2468 = p.flags_2468;
    f.flags_2472 = p.flags_2472;
    f.flags_2476 = p.flags_2476;
    f.flags_2480 = p.flags_2480;
    f.flags_2484 = p.flags_2484;
    f.category = p.category_2512;
    f.timestep = p.timestep_2604;
    f.time_on_ground = p.time_on_ground_2752;
    f.speed = p.scalar_2652;
    f.animation_up = from_bits4(p.vectors_544_560_592_608[0]);
    f.landing_angle = p.scalar_2736;
    f.deck_velocity = from_bits4(p.vectors_400_416[0]);
    f.com_velocity = from_bits4(p.vectors_544_560_592_608[3]);
    f.jump_fix_frames = std::int32_t(jump_fix_frames);
    f.deck = deck;
    f.input_board = input_board;
    f.world_to_animation = world_to_animation;
    f.closing_velocity = vector(b.closing_velocity);
    f.board_material_flags = b.collision_flags;
    //82C08494/82C08640 publish the actual counts to868/869.
    f.board_contact = b.part_contact_count != 0;
    f.wheel_contact = b.wheel_contact_count != 0;
    f.board_contact_normal = vector(b.overall_normal);
    f.opposing_contact = b.opposing_contact;
    for (std::size_t i = 0; i < f.regions_force.size(); ++i) f.regions_force[i] = c.regions[i].force;
    f.maximum_skater_force = c.maximum_skater_force;
    f.vehicle_force = c.maximum_group_8_force;
    f.group_8 = c.flags.group_8;
    f.conflicting = c.flags.conflicting;
    f.compliant = c.flags.compliant;
    f.highest_normal = c.highest_normal;
    f.pose_error = pose_error;
    if (!maximum_pose_error) {
        return rs::make_err(std::string("Wipeout requires completed Skeleton pose-error feedback"));
    }
    f.maximum_pose_error = *maximum_pose_error;
    f.flip_active = air.flip_active;
    f.flip_requested_speed = air.flip_requested_speed;
    f.system_up_y = system_up_y;
    f.grind_selected = grind_locked_to_middle;
    f.grind_normal_valid = grind_normal.has_value();
    //Unused when invalid; retain native default up without publishing a hit.
    f.grind_normal = grind_normal ? *grind_normal : std::array<float, 4>{0.0f, 1.0f, 0.0f, 0.0f};
    return f;
}

RequestInput request_input(const ProcessedPhysicsInput& p) {
    RequestInput r;
    r.flags_2468 = p.flags_2468;
    r.flags_2476 = p.flags_2476;
    r.flags_2480 = p.flags_2480;
    r.flags_2484 = p.flags_2484;
    r.animation_up_y = rs::from_bits(p.vectors_544_560_592_608[0][1]);
    r.category = p.category_2512;
    return r;
}

std::array<float, 4> vector(Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }

}  // namespace s3::game::physics::wipeout::observations
