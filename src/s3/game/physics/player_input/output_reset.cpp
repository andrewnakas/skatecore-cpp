// Port of skate-game/src/physics/player_input/output_reset.rs.
//! Represented PhysOut reset values. Original82DE53F0 copies the templates
//! built by82DE4940; scalar zero does not replace any active producer.
#include "s3/game/physics/player_input/output_reset.hpp"

namespace s3::game::physics::player_input::output_reset {

using namespace sc::player::input_phase;

void reset_outputs(PhysicalPlayerInput& out) {
    // These are host-owned observations, outside the native PhysOut templates.
    const auto surface = out.surface_default_mode;
    const auto world_grab = out.component_1832_word_1876;
    const auto anim_to_world = out.skeleton.anim_to_world_11920;
    // 82DE2EF8 clears these three represented Air324 bits; preserve the rest.
    const std::uint32_t handplant_flags = out.air.handplant_flags_324 & ~0xb0000000u;
    PhysicalPlayerInput next{};
    next.air = AirOutputFields{};
    next.air.handplant_flags_324 = handplant_flags;
    // 82DE2F70..78 loads original world-up into144.82DE304C
    // splats-1 into the final four-word trajectory descriptor lane.
    next.air.landing_normal_144 = UP;
    next.air.selected_trajectory_240 = {RawVector{0, 0, 0, 0}, RawVector{0, 0, 0, 0}, RawVector{0, 0, 0, 0},
                                        RawVector{0xbf800000u, 0xbf800000u, 0xbf800000u, 0xbf800000u}};
    next.reckoning = SystemReckoningFields{};
    next.reckoning.vector_96 = UP;  // 82DE3CC8
    next.ground = GroundOutputFields{};
    next.ground.vector_64 = UP;
    next.ground.vector_80 = UP;
    next.ground.vector_96 = UP;
    next.ground.scalar_276 = -1.0f;  // 82DE3728
    next.grinds = GrindOutputFields{};
    next.grinds.words_136_140 = {UINT32_MAX, 0};  // 82DE3518
    next.skeleton = SkeletonOutputFields{};
    next.skeleton.anim_to_world_11920 = anim_to_world;
    next.surface_default_mode = surface;
    next.component_1832_word_1876 = world_grab;
    out = next;
}

}  // namespace s3::game::physics::player_input::output_reset
