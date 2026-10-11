// Port of skate-core/src/animation/playback_tree/selection_space.rs.
#include "s3/core/animation/playback_tree/selection_space.hpp"

#include <utility>

#include "s3/core/animation/playback_tree.hpp"

namespace s3::animation::playback_tree::selection_space {

rs::Result<SelectionSpace, std::string> SelectionSpace::new_(std::vector<Parameter> parameters,
                                                             std::vector<Candidate> candidates) {
    bool invalid = parameters.size() > 10 || candidates.empty();
    for (const Candidate& c : candidates)
        if (c.values.size() != parameters.size()) invalid = true;
    if (invalid) {
        return rs::make_err(std::string("Invalid native selection space dimensions"));
    }
    std::vector<std::optional<float>> values(parameters.size(), std::nullopt);
    return SelectionSpace{std::move(parameters), std::move(candidates), std::nullopt, std::move(values), 1.0f,
                          0.0f};
}

const PlaybackTree* SelectionSpace::current() const { return selected ? candidates[*selected].tree.get() : nullptr; }

PlaybackTree* SelectionSpace::current_mut() { return selected ? candidates[*selected].tree.get() : nullptr; }

void SelectionSpace::set_time(float time) {
    requested_time = time;
    if (PlaybackTree* child = current_mut()) {
        child->set_time(time);
    }
}

void SelectionSpace::set_speed(float speed_) {
    speed = speed_;
    if (PlaybackTree* child = current_mut()) {
        child->set_speed(speed_);
    }
}

rs::Result<rs::Unit, std::string> SelectionSpace::select(Slice<const SettableAttribute> attributes) {
    for (const SettableAttribute& attribute : attributes) {
        const std::size_t n = std::min(parameters.size(), values.size());
        for (std::size_t i = 0; i < n; ++i) {
            if (parameters[i].name == attribute.name) {
                values[i] = attribute.value;
            }
        }
    }
    if (!selected) {
        std::vector<float> resolved;
        resolved.reserve(values.size());
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (!values[i]) {
                return rs::make_err("SelectionSpace parameter " + std::to_string(i) + " has no source producer");
            }
            resolved.push_back(*values[i]);
        }
        float best = rs::f32::MAX;
        std::optional<std::size_t> chosen_;
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const float d = distance(parameters, resolved, candidates[i].values);
            if (d < best) {
                best = d;
                chosen_ = i;
            }
        }
        if (!chosen_) return rs::make_err(std::string("SelectionSpace has no finite native minimum"));
        const std::size_t chosen = *chosen_;
        //82D26EE4..F88 resolves the first child record with the chosen name.
        selected.reset();
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            if (candidates[i].name == candidates[chosen].name) {
                selected = i;
                break;
            }
        }
    }
    return rs::Unit{};
}

rs::Result<bool, std::string> SelectionSpace::set_attributes(Slice<const SettableAttribute> attributes) {
    auto s = select(attributes);
    if (s.is_err()) return rs::make_err(s.unwrap_err());
    const float speed_ = speed;
    if (PlaybackTree* child = current_mut()) {
        auto r = child->set_attributes(attributes);
        if (r.is_err()) return r;
        child->set_speed(speed_);
    }
    return true;
}

float distance(Slice<const Parameter> parameters, Slice<const float> values, Slice<const float> candidate) {
    float even = 0.0f;
    float odd = 0.0f;
    const std::size_t pairs = parameters.size() / 2 * 2;
    for (std::size_t i = 0; i < pairs; i += 2) {
        float d = delta(parameters[i], values[i], candidate[i]);
        even = rs::mul_add(parameters[i].weight * d, d, even);
        d = delta(parameters[i + 1], values[i + 1], candidate[i + 1]);
        odd = rs::mul_add(parameters[i + 1].weight * d, d, odd);
    }
    const float sum = odd + even;
    if (pairs < parameters.size()) {
        const float d = delta(parameters[pairs], values[pairs], candidate[pairs]);
        return sum + (parameters[pairs].weight * d) * d;
    }
    return sum + 0.0f;
}

float delta(const Parameter& parameter, float value, float candidate) {
    const float range = parameter.maximum - parameter.minimum;
    if (range > rs::from_bits(0x37800000)) {
        candidate /= range;
        value /= range;
    }
    if (parameter.mode == 1 && value > candidate) {
        return (1.0f - value) + candidate;
    }
    if (parameter.mode == 2) {
        const float difference = rs::abs(value - candidate);
        const float wrapped = 1.0f - difference;
        return difference - wrapped >= 0.0f ? wrapped : difference;
    }
    return candidate - value;
}

}  // namespace s3::animation::playback_tree::selection_space
