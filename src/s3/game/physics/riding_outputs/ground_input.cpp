// Port of skate-game/src/physics/riding_outputs/ground_input.rs.
//! Snapshot inputs copied by original TU3 82D8E5C0 for the Ground job.
#include "s3/game/physics/riding_outputs/ground_input.hpp"

namespace s3::game::physics::riding_outputs::ground_input {

GroundPacketInputs GroundPacketInputs::from_processed(const sc::player::input_phase::ProcessedPhysicsInput& input) {
    auto vector = [](std::array<std::uint32_t, 4> bits) {
        const auto value = rs::from_bits(bits);
        return sc::math::Vector3::new_(value[0], value[1], value[2]);
    };
    GroundPacketInputs out{};
    // 82D8E790: Processed464 -> input720.
    out.wheel_normal = vector(input.vectors_464_480_496_512_528[0]);
    // 82D8E79C: Processed528 -> input736. Board112 is a retained
    // acceleration-derived normal, NOT CollisionInfo's overall normal.
    out.dynamic_up = vector(input.vectors_464_480_496_512_528[4]);
    out.speed = input.scalar_2652;           // 82D8E7C0 -> input772.
    out.absolute_speed = input.scalar_2616;  // 82D8E7D8 -> input780.
    out.wheel_count = static_cast<std::int32_t>(input.wheel_count_2556);  // 82D8E924 -> input804.
    return out;
}

}  // namespace s3::game::physics::riding_outputs::ground_input
