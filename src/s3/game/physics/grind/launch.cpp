// Port of skate-game/src/physics/grind/launch.rs.
#include "s3/game/physics/grind/launch.hpp"

#include "s3/core/math.hpp"

namespace s3::game::physics::grind::launch {

std::array<float, 4> apply(::s3::physics::board_runtime::BoardRuntime& board,
                           ::s3::physics::grind_contact::manager::Jumper& jumper, std::uint32_t& flags_2476,
                           ::s3::physics::grind_forces::launch::Input input) {
    const auto output = ::s3::physics::grind_forces::launch::launch(jumper, input);
    const auto v = output.velocity;
    // Original SetVelocity writes every board part, not only the deck. It
    // neither resets angular velocity nor teleports the board's transforms.
    for (auto& body : board.bodies_mut()) {
        body.rates.linear_velocity = ::s3::math::Vector3::new_(v[0], v[1], v[2]);
    }
    jumper = output.jumper;
    flags_2476 |= output.flags_2476_to_or;
    return v;
}

}  // namespace s3::game::physics::grind::launch
