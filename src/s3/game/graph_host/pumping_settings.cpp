// Port of skate-game/src/graph_host/pumping_settings.rs.
#include "s3/game/graph_host/pumping_settings.hpp"

namespace s3::game::graph_host::pumping_settings {

rs::Result<s3::animation::pumping_channel::Settings, std::string> load(const s3::data::Collections& data) {
    auto w = data.words<16>("anim_motion", "anim_pump", "pump_amplify");
    if (!w.ok) return rs::make_err(w.error);
    s3::animation::pumping_channel::Settings s{};
    s.amplify.x = rs::from_fn<8>([&](std::size_t i) { return rs::from_bits(w.value[i]); });
    s.amplify.y = rs::from_fn<8>([&](std::size_t i) { return rs::from_bits(w.value[8 + i]); });
#define S3_TRY_FLOAT(field, name)                                       \
    {                                                                   \
        auto r = data.float_("anim_motion", "anim_pump", name);         \
        if (!r.ok) return rs::make_err(r.error);                        \
        field = r.value;                                                \
    }
    S3_TRY_FLOAT(s.input_blend, "pump_prop_blend");
    S3_TRY_FLOAT(s.maximum_physics_pump, "max_phys_pump");
    S3_TRY_FLOAT(s.new_pump_threshold, "new_pump_thresh");
    S3_TRY_FLOAT(s.blend_in, "pump_blendin");
    S3_TRY_FLOAT(s.blend_out, "pump_blendout");
#undef S3_TRY_FLOAT
    return s;
}

}  // namespace s3::game::graph_host::pumping_settings
