// Port of skate-game/src/physics/grind/lifecycle.rs.
#include "s3/game/physics/grind/lifecycle.hpp"

#include "s3/core/physics/board.hpp"
#include "s3/core/rs.hpp"

namespace s3::game::physics::grind::lifecycle {

using ::s3::physics::board::BodyId;

void enter(BoardRuntime& board, std::uint8_t& animated_290, bool& skeleton_16505, SkeletonCollisionMode& collision) {
    board.hook_mut().drive.disable_animation(animated_290);
    skeleton_16505 = true;
    //82D91330 disables these four volumes permanently (counter0), not the
    //six-body/two-frame collision suppression used by KnownAir grind assistance.
    for (const std::size_t part : FEET) {
        collision.parts[part].volume_group = 4;
        collision.parts[part].enabled = false;
        collision.disable_count[part] = 0;
    }
    board.bodies_mut()[::s3::physics::board::index(BodyId::Deck)].inertia.angular_drag = rs::from_bits(0x40666665u);
    // No ResetSpin call exists in this Enter.
}

void exit(BoardRuntime& board, std::uint8_t& animated_290, SkeletonCollisionMode& collision,
          std::array<bool, 4> bone_has_collision, float standard_angular_drag) {
    for (std::size_t k = 0; k < 4; ++k) {
        collision.normal_bone(FEET[k], bone_has_collision[k]);
    }
    board.bodies_mut()[::s3::physics::board::index(BodyId::Deck)].inertia.angular_drag = standard_angular_drag;
    board.hook_mut().drive.disable_animation(animated_290);
    clear_wheel_spin(board);
}

///82D3F710. These are actual wheel collision bytes844..847.
void manage_wheel_spin(BoardRuntime& board, std::array<bool, 4> contact) {
    auto& bodies = board.bodies_mut();
    for (std::size_t k = 0; k < 4; ++k) {
        bodies[k].inertia.angular_drag = (contact[k] ? 0.5f : 0.0f) * rs::from_bits(0x426fffffu);
    }
}

///82D3F7D0/S2 82D85228 SetPartAngularDrag; ZIP's linear_drag write was wrong.
void clear_wheel_spin(BoardRuntime& board) {
    auto& bodies = board.bodies_mut();
    for (std::size_t k = 0; k < 4; ++k) {
        bodies[k].inertia.angular_drag = 0.0f;
    }
}

}  // namespace s3::game::physics::grind::lifecycle
