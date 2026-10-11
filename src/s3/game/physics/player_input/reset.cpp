// Port of skate-game/src/physics/player_input/reset.rs.
//! Selective reset of the represented ProcessedPhysIn fields, TU3 82BF9EF0.
//! Packet buffers and retained flag lanes deliberately survive this reset.
#include "s3/game/physics/player_input/reset.hpp"

namespace s3::game::physics::player_input::reset {

using sc::player::input_phase::ProcessedPhysicsInput;

void reset_processed(ProcessedPhysicsInput& input) {
    const auto external = input.external_physics_1616;
    const auto vector_832 = input.vectors_720_784_800_816_832_864[4];
    const std::uint32_t one = rs::to_bits(1.0f);
    ProcessedPhysicsInput reset{};
    reset.effective_anim_transform_192 = {{
        {one, 0, 0, 0},
        {0, one, 0, 0},
        {0, 0, one, 0},
        {0, 0, 0, 0},
    }};
    reset.vector_1520 = input.vector_1520;
    reset.matrix_1536 = input.matrix_1536;
    reset.byte_1600 = input.byte_1600;
    reset.external_physics_1616 = external;
    reset.probe_1792 = input.probe_1792;
    reset.flags_2468 = 0x2000u | (input.flags_2468 & 8u);
    reset.flags_2488 = input.flags_2488 & 0x001fffffu;
    reset.state_variant_index_2528 = 1;
    reset.grind_words_2532_2536 = {UINT32_MAX, 0};
    reset.timestep_2604 = rs::from_bits(0x3c888889u);
    reset.gravity_2648 = rs::from_bits(0xc11ccccdu);
    reset.actor_query_2952 = UINT32_MAX;
    reset.external_physics_1616.flags &= 0x01ffffffu;
    reset.vectors_544_560_592_608[0] = {0, one, 0, 0};
    // Raw 82BFA1D0 and 82BFA1D8 both store to +816; +832 is retained.
    reset.vectors_720_784_800_816_832_864[4] = vector_832;
    input = reset;
}

}  // namespace s3::game::physics::player_input::reset
