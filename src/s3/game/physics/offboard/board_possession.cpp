// Port of skate-game/src/physics/offboard/board_possession.rs
#include "s3/game/physics/offboard/board_possession.hpp"

#include <algorithm>

namespace s3::game::physics::offboard::board_possession {

void Materials::set(const std::array<RetailContactMaterial, 3>& values) {
    *wheels = values[0];
    *trucks = values[1];
    *deck = values[2];
}

void VolumeFlags::set_enabled(bool enabled) {
    deck = enabled;
    trucks = enabled;
    wheels = enabled;
    std::fill(deck_children.begin(), deck_children.end(), enabled);
}

void Effects::enable_animation_soft() { board->hook_mut().drive.enable_animation_soft(*animated_290); }
void Effects::enable_animation_angular_only() { board->hook_mut().drive.enable_angular_only(*animated_290); }
void Effects::disable_animation() { board->hook_mut().drive.disable_animation(*animated_290); }
void Effects::disable_linear_drive() { board->hook_mut().drive.disable_linear(); }
void Effects::standard_board() {
    *wiping_out = false;
    board->bodies_mut()[6].inertia.angular_drag = standard_angular_drag;
    board->set_collision_group(4);
    materials.set(standard_materials);
    collision_volumes(true);
}
void Effects::released_board() {
    *wiping_out = true;
    board->bodies_mut()[6].inertia.angular_drag = rs::from_bits(0x416fffffu);
    board->set_collision_group(7);
    materials.set({released_material, released_material, released_material});
    collision_volumes(true);
}
void Effects::collision_volumes(bool enabled) { volumes->set_enabled(enabled); }
void Effects::clear_alignment() { *alignment_active = false; }
void Effects::alignment(Alignment value) {
    *alignment_ = value;
    *alignment_active = true;
}
void Effects::velocity(Vector value) {
    for (auto& body : board->bodies_mut()) body.rates.linear_velocity = xyz(value);
}
void Effects::position(Vector value) {
    auto target = board->part_transforms()[6];
    target.translation = xyz(value);
    board->set_transform(target);
}
void Effects::hook_frame(Frame value) { board->set_hook_transform(affine(value)); }
void Effects::target_position_velocity(Vector value) {
    const Vector3 center = board->bodies()[6].rates.position;
    const Vector delta{value[0] - center.x, value[1] - center.y, value[2] - center.z, 0.0f};
    Vector scaled{};
    for (std::size_t i = 0; i < 4; ++i) scaled[i] = delta[i] * (1.0f / processed_dt_2604);
    velocity(scaled);
}
void Effects::torque(Vector value) {
    auto& deck = board->bodies_mut()[6];
    const auto& c = deck.rates.world_inverse_inertia.columns;
    const std::array<Vector, 3> inertia{Vector{c[0][0], c[0][1], c[0][2], 0.0f}, Vector{c[1][0], c[1][1], c[1][2], 0.0f},
                                        Vector{c[2][0], c[2][1], c[2][2], 0.0f}};
    const Vector delta =
        sc::player::offboard::board_possession::angular::acceleration_delta(value, vector(deck.rates.angular_velocity),
                                                                            inertia);
    const Vector3 a = deck.rates.torque_acceleration;
    deck.rates.torque_acceleration = Vector3::new_(a.x + delta[0], a.y + delta[1], a.z + delta[2]);
    deck.rates.cool_down = 0;
}

Vector3 xyz(Vector v) { return Vector3::new_(v[0], v[1], v[2]); }
Vector vector(Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }
RetailAffineTransform affine(const Frame& v) {
    RetailAffineTransform out{};
    for (std::size_t i = 0; i < 3; ++i) out.basis.columns[i] = {v[i][0], v[i][1], v[i][2]};
    out.translation = xyz(v[3]);
    return out;
}

}  // namespace s3::game::physics::offboard::board_possession
