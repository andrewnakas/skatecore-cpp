// Port of skate-core/src/camera/anchors.rs.
#include "s3/core/camera/anchors.hpp"

#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera {

void Anchors::update(const AnchorInputs& input) {
    // Publisher82DF7790 always passes this fixed timestep to every anchor,
    // independently of CameraMan's render update timestep.
    const float dt = rs::from_bits(0x3c888889);
    const float inverse = vector_tracker::refined_reciprocal(dt);
    const F4 board = rs::from_fn<4>([&](std::size_t i) {
        return rs::mul_add(input.board_offset_direction[i], rs::from_bits(0x3e99999a), input.board_position[i]);
    });
    const F4 air = rs::from_fn<4>([&](std::size_t i) {
        return input.center_of_mass[i] - input.skeleton_root_up[i] * rs::from_bits(0x3ea8f5c3);
    });
    // GrindAnchor82DF2C78 retains and advects its point after leaving a rail.
    if (input.reset || input.grinding) {
        const F4 point = input.reset ? input.board_position : input.grind_point;
        this->grind_position =
            rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(input.board_offset_direction[i], 0.5f, point[i]); });
    } else {
        this->grind_position = rs::from_fn<4>(
            [&](std::size_t i) { return rs::mul_add(input.board_velocity[i], dt, this->grind_position[i]); });
    }
    const std::array<F4, 7> positions = {
        board, input.center_of_mass, air, this->grind_position, F4{}, input.damped_center_of_mass, F4{},
    };
    for (std::size_t index = 0; index < 7; ++index) {
        AnchorState& state = this->entries[index];
        const F4& position = positions[index];
        // Common82DF2950: first publish position, then derive velocity;
        // acceleration comes from a separate getter and is not differenced.
        const F4 previous = state.position;
        state.position = position;
        state.velocity = input.reset ? F4{}
                                     : rs::from_fn<4>([&](std::size_t i) { return inverse * (position[i] - previous[i]); });
        state.acceleration = (index == 0 || index == 2 || index == 3) ? input.board_acceleration : F4{};
    }
}

}  // namespace s3::camera
