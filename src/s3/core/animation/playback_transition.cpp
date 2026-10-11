// Port of skate-core/src/animation/playback_transition.rs.
#include "s3/core/animation/playback_transition.hpp"

#include <utility>

#include "s3/core/animation/playback_tree.hpp"

namespace s3::animation::playback_transition {

PlaybackTransition PlaybackTransition::new_(PlaybackTree from, PlaybackTree to, TransitionSettings settings) {
    return PlaybackTransition{Box<PlaybackTree>(std::move(from)), Box<PlaybackTree>(std::move(to)), settings, 0.0f,
                              false};
}

bool PlaybackTransition::complete() const {
    if (settings.kind == 4) {
        return sequence_complete;
    }
    return elapsed > settings.seconds;
}

void PlaybackTransition::advance(float dt, float phase, AdvanceResult& property) {
    AdvanceResult discarded{false, -1.0f, -1.0f};
    if (settings.kind == 4) {
        if (sequence_complete) {
            to->advance(dt, phase, property);
        } else {
            from->advance(dt, phase, discarded);
            if (discarded.crossed_end) {
                sequence_complete = true;
                to->advance(discarded.overshoot, phase, property);
            } else {
                property.crossed_end = false;
                property.remaining_before_wrap = to->length() + discarded.remaining_before_wrap;
            }
        }
        return;
    }
    if (settings.matching != 1) {
        from->advance(dt, phase, discarded);
    }
    if (settings.matching == 3) {
        const float length = to->length();
        const float time = from->time();
        to->set_time(time - length >= 0.0f ? length : time);
    } else if (settings.matching == 2 && from->length() > 0.0f) {
        to->set_time(to->length() * (from->time() / from->length()));
    } else {
        to->advance(dt, phase, property);
    }
    elapsed += dt;
}

rs::Result<bool, std::string> PlaybackTransition::set_attributes(Slice<const SettableAttribute> attributes) {
    auto result = to->set_attributes(attributes);
    if (result.is_err()) return result;
    if (settings.kind == 4 && !sequence_complete) {
        return from->set_attributes(attributes);
    }
    return result;
}

const PlaybackTree& PlaybackTransition::attributes_child() const {
    if (settings.kind == 4 && !sequence_complete) {
        return *from;
    }
    return *to;
}

rs::Result<std::vector<AnimationAttribute>, std::string> PlaybackTransition::attributes(std::uint32_t mask) const {
    return attributes_child().attributes(mask);
}

rs::Result<bool, std::string> PlaybackTransition::query_attribute(AttributeName name, std::uint32_t mask,
                                                                  AnimationAttribute& output) const {
    return attributes_child().query_attribute(name, mask, output);
}

float PlaybackTransition::weight() const {
    const float value = elapsed / settings.seconds;
    const float lower = -value >= 0.0f ? 0.0f : value;
    const float w = 1.0f - lower >= 0.0f ? lower : 1.0f;
    if (settings.seconds < rs::from_bits(0x3d4ccccd) || w > 1.0f || w < 0.0f) {
        return w;
    }
    const float square = w * w;
    const float cube = square * w;
    return rs::mul_add(square, 3.0f, -(cube * 2.0f));
}

rs::Result<bool, std::string> PlaybackTransition::evaluate(Evaluation parameters, bool enabled,
                                                           std::vector<PoseCommand>& output) {
    if (settings.kind == 4) {
        return sequence_complete ? to->evaluate(parameters, enabled, output)
                                 : from->evaluate(parameters, enabled, output);
    }
    const float w = weight();
    auto f = from->evaluate(parameters, enabled, output);
    if (f.is_err()) return f;
    auto t = to->evaluate(parameters, enabled, output);
    if (t.is_err()) return t;
    const bool from_ok = f.unwrap();
    const bool to_ok = t.unwrap();
    if (enabled && from_ok && to_ok) {
        if (settings.kind == 3) {
            output.push_back(PoseCommand::ChannelBlend{w, settings.use_channels_from_weights});
        } else {
            output.push_back(PoseCommand::Blend{w});
        }
    }
    return from_ok || to_ok;
}

}  // namespace s3::animation::playback_transition
