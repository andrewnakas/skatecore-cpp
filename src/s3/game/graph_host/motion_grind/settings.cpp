// Port of skate-game/src/graph_host/motion_grind/settings.rs (module `motion_grind::settings`).
#include "s3/game/graph_host/motion_grind/settings.hpp"

namespace s3::game::graph_host::motion_grind::settings {

rs::Result<Settings, std::string> Settings::load(const s3::data::Collections& d) {
    Settings s{};
    const struct {
        float* out;
        const char* key;
        const char* name;
    } fields[] = {
        {&s.fade.response, "grind_twist", "twist_smoothing"},
        {&s.fade.input_scale, "grind_twist", "twist_sensitivity"},
        {&s.fade.acceleration, "grind_twist", "twist_max_delta_delta"},
        {&s.fade.maximum_step, "grind_twist", "twist_max_delta"},
        {&s.height[0], "grind_height", "min_grind_disttocog"},
        {&s.height[1], "grind_height", "max_grind_disttocog"},
        {&s.height[2], "grind_height", "grind_disttocog_speed"},
    };
    for (const auto& field : fields) {
        auto value = d.float_("anim_motion", field.key, field.name);
        if (!value.ok) return rs::make_err(value.error);
        *field.out = value.value;
    }
    return s;
}

}  // namespace s3::game::graph_host::motion_grind::settings
