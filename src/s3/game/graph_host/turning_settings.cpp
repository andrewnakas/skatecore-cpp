// Port of skate-game/src/graph_host/turning_settings.rs.
#include "s3/game/graph_host/turning_settings.hpp"

#include "s3/core/input/turn_remap.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::game::graph_host::turning_settings {

using s3::point_graph::PointGraph;

namespace {
rs::Result<PointGraph<16>, std::string> curve16(const s3::data::Collections& data, const char* name) {
    auto words = data.words<36>("anim_carving", "default", name);
    if (!words.ok) return rs::make_err(words.error);
    PointGraph<16> out;
    out.x = rs::from_fn<16>([&](std::size_t i) { return rs::from_bits(words.value[4 + i]); });
    out.y = rs::from_fn<16>([&](std::size_t i) { return rs::from_bits(words.value[20 + i]); });
    return out;
}
rs::Result<PointGraph<8>, std::string> curve8(const s3::data::Collections& data, const char* name) {
    auto words = data.words<20>("anim_carving", "default", name);
    if (!words.ok) return rs::make_err(words.error);
    PointGraph<8> out;
    out.x = rs::from_fn<8>([&](std::size_t i) { return rs::from_bits(words.value[4 + i]); });
    out.y = rs::from_fn<8>([&](std::size_t i) { return rs::from_bits(words.value[12 + i]); });
    return out;
}
}  // namespace

rs::Result<s3::input::set_turning::Settings, std::string> load(const s3::data::Collections& data) {
#define S3_TRY_GRAPH(field, expr)                                       \
    {                                                                   \
        auto r = (expr);                                                \
        if (r.is_err()) return rs::make_err(r.unwrap_err());            \
        field = r.unwrap();                                             \
    }
#define S3_TRY_FLOAT(field, cls, key, name)                             \
    {                                                                   \
        auto r = data.float_(cls, key, name);                           \
        if (!r.ok) return rs::make_err(r.error);                        \
        field = r.value;                                                \
    }
    s3::input::set_turning::Settings s{};
    // Mode0 selects layout288/432/744; mode1 selects0/144/740.
    S3_TRY_GRAPH(s.remaps[0].magnitude, curve16(data, "quickMagMap"));
    S3_TRY_GRAPH(s.remaps[0].angle, curve16(data, "quickAngleMap"));
    S3_TRY_FLOAT(s.remaps[0].angle_offset, "anim_carving", "default", "quickAngleOffset");
    S3_TRY_GRAPH(s.remaps[1].magnitude, curve16(data, "slowMagMap"));
    S3_TRY_GRAPH(s.remaps[1].angle, curve16(data, "slowAngleMap"));
    S3_TRY_FLOAT(s.remaps[1].angle_offset, "anim_carving", "default", "slowAngleOffset");
    S3_TRY_GRAPH(s.speed_tuck, curve8(data, "speed_tuck"));
    S3_TRY_GRAPH(s.blend, curve8(data, "blend_lean_in"));
    S3_TRY_FLOAT(s.speed_threshold, "anim_carving", "default", "speed_tuck_start");
    S3_TRY_FLOAT(s.maximum_delta, "anim_carving", "default", "clamp_curr_lean");
    // Global332 is anim_motion/power_slide, layout1680.
    S3_TRY_FLOAT(s.override_turn, "anim_motion", "power_slide", "slide_turn_value");
#undef S3_TRY_GRAPH
#undef S3_TRY_FLOAT
    return s;
}

}  // namespace s3::game::graph_host::turning_settings
