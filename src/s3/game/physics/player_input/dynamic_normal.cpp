// Port of skate-game/src/physics/player_input/dynamic_normal.rs.
//! Stock bindings for original82C02388. Hashes were matched to the actual
//! collection names; XML layoutC85F0-C8190 verifies curve offset1120.
#include "s3/game/physics/player_input/dynamic_normal.hpp"

namespace s3::game::physics::player_input::dynamic_normal {

using sc::physics::board_dynamic_normal::DynamicNormalSettings;

rs::Result<DynamicNormalSettings, std::string> settings(const sd::Collections& data) {
    auto f = [&](const char* field) { return data.float_("physics_reckoning", "default", field); };
    S3G_TRY(const auto words, data.words<16>("physics_reckoning", "default", "DynamicSpeedMaxDeltaGraphZ"));
    const auto values = rs::from_bits(words);
    DynamicNormalSettings out;
    S3G_TRY(out.speed_damping, f("DynamicSpeedDamping"));             // AB8F93BA080E0CB3
    S3G_TRY(out.up_vector_damping, f("DynamicUpVectorDamping"));      // 9F15310DBBE96B8C
    S3G_TRY(out.maximum_delta, f("DynamicSpeedMaxDelta"));            // 30BF4DA4BD7AE00A
    S3G_TRY(out.speed_scale, f("DynamicSpeedMaxDeltaXScale"));        // 20F1ECA637708494
    for (std::size_t i = 0; i < 8; ++i) {
        out.maximum_delta_vs_speed.x[i] = values[i];
        out.maximum_delta_vs_speed.y[i] = values[8 + i];
    }
    return out;
}

}  // namespace s3::game::physics::player_input::dynamic_normal
