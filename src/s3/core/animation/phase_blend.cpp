// Port of skate-core/src/animation/phase_blend.rs.
#include "s3/core/animation/phase_blend.hpp"

#include <utility>

#include "s3/core/animation/playback_attributes.hpp"
#include "s3/core/animation/playback_tree.hpp"

namespace s3::animation::phase_blend {

using output::attributes::MotionGraphAttribute;

rs::Result<PhaseBlend, std::string> PhaseBlend::new_(AttributeName parameter, std::vector<PlaybackTree> children) {
    if (children.size() < 2) {
        return rs::make_err(std::string("PhaseBlend requires at least two authored children"));
    }
    const std::size_t count = children.size();
    PhaseBlend out;
    out.parameter = parameter;
    out.length = children[0].length();
    out.children = std::move(children);
    out.order.resize(count);
    for (std::size_t i = 0; i < count; ++i) out.order[i] = i;
    out.values.assign(count, 0.0f);
    out.left = 0;
    out.right = 1;
    out.weight = 0.0f;
    out.parameter_value = rs::f32::MAX;
    out.normalized = true;
    out.cull_threshold = 0.0f;
    out.time = 0.0f;
    return out;
}

void PhaseBlend::set_time(float time_) {
    const float phase_ = time_ / length;
    const float left_time = children[left].length() * phase_;
    const float right_time = children[right].length() * phase_;
    children[left].set_time(left_time);
    children[right].set_time(right_time);
    time = time_;
}

float PhaseBlend::phase() const { return time / (-length >= 0.0f ? 1.0f : length); }

void PhaseBlend::update_length(float phase_) {
    length = rs::mul_add(children[right].length(), weight, children[left].length() * (1.0f - weight));
    set_time(length * phase_);
}

void PhaseBlend::set_speed(float speed) {
    const float phase_ = phase();
    for (std::size_t index : order) {
        children[index].set_speed(speed);
    }
    update_length(phase_);
}

void PhaseBlend::advance(float dt, float phase_, AdvanceResult& property) {
    const float left_length = children[left].length();
    const float right_length = children[right].length();
    const float fraction = dt / length;
    children[left].advance(fraction * left_length, phase_, property);
    AdvanceResult discarded{false, -1.0f, -1.0f};
    children[right].advance(fraction * right_length, phase_, discarded);
    const float inverse_left = 1.0f / left_length;
    time = (children[left].time() * inverse_left) * length;
    property.remaining_before_wrap = (property.remaining_before_wrap * inverse_left) * length;
    property.overshoot = (inverse_left * length) * property.overshoot;
}

rs::Result<bool, std::string> PhaseBlend::set_attributes(Slice<const SettableAttribute> attributes) {
    bool changed = false;
    for (std::size_t index : order) {
        auto r = children[index].set_attributes(attributes);
        if (r.is_err()) return r;
        changed |= r.unwrap();
    }
    for (const SettableAttribute& attribute : attributes) {
        if (attribute.name == parameter) {
            if (attribute.normalized != normalized ||
                !(rs::abs(attribute.value - parameter_value) < rs::from_bits(0x38d1b717))) {
                parameter_value = attribute.value;
                normalized = attribute.normalized;
                changed = true;
            }
            break;
        }
    }
    if (!changed) {
        return false;
    }
    for (std::size_t index : order) {
        AnimationAttribute attribute = MotionGraphAttribute{parameter, 0.0f}.to_animation();
        // Native deliberately ignores success; partial output from nested
        // GetAttribute remains visible, and a complete miss retains zero.
        auto r = children[index].query_attribute(parameter, 15, attribute);
        if (r.is_err()) return r;
        const auto bits = attribute.payload._0[0];
        if (!bits) return rs::make_err(std::string("Uninitialized PhaseBlend parameter"));
        values[index] = rs::from_bits(*bits);
    }
    for (std::size_t end = order.size(); end-- > 1;) {
        for (std::size_t i = 0; i < end; ++i) {
            if (values[order[i + 1]] < values[order[i]]) {
                std::swap(order[i], order[i + 1]);
            }
        }
    }
    const float minimum = values[order[0]];
    const float maximum = values[order.back()];
    const float target = normalized ? rs::mul_add(maximum - minimum, parameter_value, minimum) : parameter_value;
    std::size_t low = 0;
    std::size_t count = order.size();
    while (count != 0) {
        const std::size_t step = count / 2;
        const std::size_t middle = low + step;
        if (values[order[middle]] < target) {
            low = middle + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    const std::size_t hi = order.size() - 1;
    if (1 > hi) S3_PANIC("assertion failed: min <= max");
    const std::size_t upper = low < 1 ? 1 : (low > hi ? hi : low);
    const std::size_t left_ = order[upper - 1];
    const std::size_t right_ = order[upper];
    const float lower_value = values[left_];
    const float upper_value = values[right_];
    const float delta = upper_value - lower_value;
    const float lower = lower_value - target >= 0.0f ? lower_value : target;
    const float clamped = upper_value - lower >= 0.0f ? lower : upper_value;
    const float w = (clamped - lower_value) / (delta == 0.0f ? 1.0f : delta);
    const float lower_w = -w >= 0.0f ? 0.0f : w;
    weight = 1.0f - lower_w >= 0.0f ? lower_w : 1.0f;
    const float phase_ = phase();
    left = left_;
    right = right_;
    update_length(phase_);
    return true;
}

rs::Result<std::vector<AnimationAttribute>, std::string> PhaseBlend::attributes(std::uint32_t mask) const {
    if (weight < cull_threshold) {
        return children[left].attributes(mask);
    }
    if (weight > 1.0f - cull_threshold) {
        return children[right].attributes(mask);
    }
    auto l = children[left].attributes(mask);
    if (l.is_err()) return l;
    auto r = children[right].attributes(31);
    if (r.is_err()) return r;
    return playback_attributes::intersection(std::move(l.unwrap()), r.unwrap(), weight);
}

rs::Result<bool, std::string> PhaseBlend::query_attribute(AttributeName name, std::uint32_t mask,
                                                          AnimationAttribute& output) const {
    if (weight < cull_threshold) {
        return children[left].query_attribute(name, mask, output);
    }
    if (weight > 1.0f - cull_threshold) {
        return children[right].query_attribute(name, mask, output);
    }
    auto l = children[left].query_attribute(name, mask, output);
    if (l.is_err()) return l;
    AnimationAttribute right_attr = MotionGraphAttribute{name, 0.0f}.to_animation();
    auto r = children[right].query_attribute(name, 31, right_attr);
    if (r.is_err()) return r;
    if (l.unwrap() && r.unwrap()) {
        auto b = playback_attributes::blend(output, right_attr, weight);
        if (b.is_err()) return rs::make_err(b.unwrap_err());
        return true;
    }
    return false;
}

rs::Result<bool, std::string> PhaseBlend::evaluate(Evaluation parameters, bool enabled,
                                                   std::vector<PoseCommand>& output) {
    cull_threshold = parameters.cull_threshold;
    const float c = parameters.cull_threshold;
    const float lower = c - weight >= 0.0f ? c : weight;
    const float upper = 1.0f - c;
    const float clamped = upper - lower >= 0.0f ? lower : upper;
    const float t = (clamped - c) / rs::mul_add(-c, 2.0f, 1.0f);
    const float square = t * t;
    float w;
    if (!(t > 0.5f) && left == order[0]) {
        w = rs::mul_add(-t, 4.0f, 4.0f) * square;
    } else if (t > 0.5f && right == order.back()) {
        const float four = t * 4.0f;
        w = rs::mul_add(8.0f - four, square, -four) + 1.0f;
    } else {
        w = weight;
    }
    auto l = children[left].evaluate(parameters, enabled && w < 1.0f, output);
    if (l.is_err()) return l;
    auto r = children[right].evaluate(parameters, enabled && w > 0.0f, output);
    if (r.is_err()) return r;
    if (l.unwrap() && r.unwrap()) {
        output.push_back(PoseCommand::Blend{w});
    }
    return l.unwrap() || r.unwrap();
}

}  // namespace s3::animation::phase_blend
