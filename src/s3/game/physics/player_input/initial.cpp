// Port of skate-game/src/physics/player_input/initial.rs.
//! Original82DB18A0 initialization of the represented input owner. Native
//! opaque allocation flags are replaced by zero-initialized host storage.
#include "s3/game/physics/player_input/initial.hpp"

namespace s3::game::physics::player_input::initial {

using sc::player::input_phase::PlayerInputState;
using sc::player::input_phase::StateVariantFields;

rs::Result<PlayerInputState, std::string> player(const sd::Collections& data) {
    PlayerInputState result{};
    // 82DB1A9C inserts E0080000; active human actor adds bit18 at307C.
    result.flags_1296 = 0xe00c0000u;
    result.ground_history_frames_1304 = 100;
    // 82DB19C0: the external packet's ninth vector is the -1 sentinel.
    const std::uint32_t minus_one = rs::to_bits(-1.0f);
    result.external_physics_cache_1008.vectors[8] = {minus_one, minus_one, minus_one, minus_one};
    static const char* const KEYS[] = {"easy", "normal", "hardcore", "motorized", "test"};
    for (std::size_t index = 0; index < 5; ++index) {
        // Original full64 keys at82DB1B08..1B78; stock XML layout+60.
        S3G_TRY(const bool enabled, data.boolean("physics_mode", KEYS[index], "Hash_5548109D7B0CB70C"));
        StateVariantFields v{};
        v.surface_override_enabled_60 = enabled ? 1 : 0;
        result.state_variants_1408[index] = v;
    }
    return result;
}

}  // namespace s3::game::physics::player_input::initial
