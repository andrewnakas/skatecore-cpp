// Port of skate-core/src/animation/playback_parameters.rs.
#include "s3/core/animation/playback_parameters.hpp"

#include <algorithm>

#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::animation::playback_parameters {

rs::Result<rs::Unit, std::string> PlaybackParameter::update(bool beginning, ParameterInputs& inputs,
                                                            AttributeSink& output) const {
    using skeleton_input::name::encode;
    AttributeName name;
    std::optional<float> value;
    bool normalized_ = false;
    if (const auto* s = std::get_if<ParameterSource::MotionIntent>(&source.value)) {
        name = rename ? *rename : encode(s->_0);
        const std::optional<float> found = inputs.motion_intent(s->_0);
        value = found ? found : default_value;
        normalized_ = normalized;
    } else if (const auto* s = std::get_if<ParameterSource::FilteredIntent>(&source.value)) {
        const std::optional<float> found = inputs.filtered_intent(s->_0);
        name = rename ? *rename : encode(s->_0);
        value = found ? found : default_value;
        //82BB4908..491C goes through4AF8 and explicitly passes0.
        normalized_ = found.has_value() && normalized;
    } else {
        const AttributeName src = std::get<ParameterSource::LastAnimation>(source.value)._0;
        if (!beginning) {
            return rs::Unit{};
        }
        auto last = inputs.last_attribute(src);
        if (last.is_err()) return rs::make_err(last.unwrap_err());
        const std::optional<AnimationAttribute>& attribute = last.unwrap();
        if (attribute) {
            if (attribute->kind == 0 || attribute->kind == 2) {
                const std::optional<std::uint32_t> bits = attribute->payload._0[0];
                if (!bits) return rs::make_err(std::string("Last animation scalar payload is uninitialized"));
                value = rs::from_bits(*bits);
            } else {
                return rs::Unit{};
            }
        } else {
            value = default_value;
        }
        name = rename ? *rename : src;
        normalized_ = false;
    }
    if (value) {
        output.set_attribute(SettableAttribute{name, *value, normalized_, -1});
    }
    return rs::Unit{};
}

std::array<std::uint32_t, 6> intent_key(std::string_view text) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(text.data());
    const std::size_t len = text.size();
    const auto first = skeleton_input::name::encode(bytes, len)._0;
    std::uint32_t tail = 0;
    if (len > 30 && std::find(bytes, bytes + 30, std::uint8_t(0)) == bytes + 30) {
        tail = skeleton_input::name::encode(bytes + 30, len - 30)._0[0];
    }
    return {first[0], first[1], first[2], first[3], first[4], tail};
}

}  // namespace s3::animation::playback_parameters
