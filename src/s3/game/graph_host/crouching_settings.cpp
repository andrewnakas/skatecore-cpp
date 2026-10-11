// Port of skate-game/src/graph_host/crouching_settings.rs.
#include "s3/game/graph_host/crouching_settings.hpp"

#include "s3/core/point_graph.hpp"

namespace s3::game::graph_host::crouching_settings {

using s3::point_graph::PointGraph;

namespace {
template <std::size_t W, std::size_t N>
rs::Result<PointGraph<N>, std::string> graph(const s3::data::Collections& data, const char* key,
                                             const char* name, std::size_t x0, std::size_t y0) {
    auto w = data.words<W>("anim_motion", key, name);
    if (!w.ok) return rs::make_err(w.error);
    PointGraph<N> out;
    out.x = rs::from_fn<N>([&](std::size_t i) { return rs::from_bits(w.value[x0 + i]); });
    out.y = rs::from_fn<N>([&](std::size_t i) { return rs::from_bits(w.value[y0 + i]); });
    return out;
}
rs::Result<PointGraph<8>, std::string> negative8(const s3::data::Collections& data, const char* key,
                                                 const char* name) {
    return graph<20, 8>(data, key, name, 4, 12);
}
rs::Result<PointGraph<4>, std::string> negative4(const s3::data::Collections& data, const char* key,
                                                 const char* name) {
    return graph<12, 4>(data, key, name, 4, 8);
}
rs::Result<PointGraph<8>, std::string> plain8(const s3::data::Collections& data, const char* key,
                                              const char* name) {
    return graph<16, 8>(data, key, name, 0, 8);
}
}  // namespace

rs::Result<s3::animation::crouching::Settings, std::string> load(const s3::data::Collections& data) {
#define S3_TRY_GRAPH(field, expr)                                       \
    {                                                                   \
        auto r = (expr);                                                \
        if (r.is_err()) return rs::make_err(r.unwrap_err());            \
        field = r.unwrap();                                             \
    }
#define S3_TRY_FLOAT(field, key, name)                                  \
    {                                                                   \
        auto r = data.float_("anim_motion", key, name);                 \
        if (!r.ok) return rs::make_err(r.error);                        \
        field = r.value;                                                \
    }
    s3::animation::crouching::Settings s{};
    S3_TRY_GRAPH(s.maximum_height, negative8(data, "crouching", "crouching_max_height"));
    S3_TRY_GRAPH(s.absorption_upforce, plain8(data, "crouching", "absorption_upforce"));
    S3_TRY_GRAPH(s.pump_maxspeed, plain8(data, "crouching", "pump_maxspeed"));
    S3_TRY_FLOAT(s.minimum_height, "crouching", "crouching_min_height");
    S3_TRY_FLOAT(s.maximum_ratio, "crouching", "crouch_max_ratio");
    S3_TRY_FLOAT(s.maximum_delta_delta, "crouching", "crouch_max_delta_delta");
    S3_TRY_FLOAT(s.maximum_delta, "crouching", "crouch_max_delta");
    S3_TRY_FLOAT(s.input_blend, "crouching", "crouch_blend_input");
    S3_TRY_FLOAT(s.pump_vertical_speed, "crouching", "pump_maxspeed_yaxis");
    S3_TRY_FLOAT(s.maximum_crouch_from_deck, "crouching", "MaxCrouchFromDeckAngle");
    S3_TRY_FLOAT(s.skateboard_damping, "crouching", "absorption_skateboard_damping");
    S3_TRY_FLOAT(s.maximum_force, "crouching", "absorption_maxforce");
    S3_TRY_FLOAT(s.maximum_ground_force, "crouching", "absorption_ground_maxforce");
    S3_TRY_FLOAT(s.absorption_factor, "crouching", "absorption_factor");
    S3_TRY_GRAPH(s.auto_pump.maximum_crouch, negative4(data, "auto_pump", "auto_pump_max_crouch"));
    S3_TRY_FLOAT(s.auto_pump.sufficient_crouch, "auto_pump", "auto_pump_sufficient_crouch");
    S3_TRY_FLOAT(s.auto_pump.standing_threshold, "auto_pump", "auto_pump_standing_thresh");
    S3_TRY_FLOAT(s.auto_pump.rise_speed, "auto_pump", "auto_pump_rise_speed");
    S3_TRY_FLOAT(s.auto_pump.pump_speed, "auto_pump", "auto_pump_pump_speed");
    S3_TRY_FLOAT(s.auto_pump.potential_threshold, "auto_pump", "auto_pump_potential_thresh");
    S3_TRY_FLOAT(s.auto_pump.potential_blend, "auto_pump", "auto_pump_potential_blend");
    S3_TRY_FLOAT(s.auto_pump.intent_magnitude_start, "auto_pump", "auto_pump_intent_mag_start");
    S3_TRY_FLOAT(s.auto_pump.intent_angle_region, "auto_pump", "auto_pump_intent_angle_region");
    S3_TRY_FLOAT(s.auto_pump.crouch_time, "auto_pump", "auto_pump_crouch_time");
    S3_TRY_FLOAT(s.auto_pump.crouch_speed, "auto_pump", "auto_pump_crouch_speed");
#undef S3_TRY_GRAPH
#undef S3_TRY_FLOAT
    return s;
}

}  // namespace s3::game::graph_host::crouching_settings
