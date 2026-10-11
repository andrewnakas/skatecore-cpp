// Port of skate-core/src/animation/playback_tree/blend_space.rs.
#include "s3/core/animation/playback_tree/blend_space.hpp"

#include <optional>
#include <utility>

#include "s3/core/animation/playback_attributes.hpp"
#include "s3/core/animation/playback_tree.hpp"

namespace s3::animation::playback_tree::blend_space {

using output::attributes::MotionGraphAttribute;

std::vector<float> Simplex::coordinates(Slice<const float> point) const {
    std::vector<float> out;
    out.reserve(children.size());
    for (std::size_t i = 0; i < children.size(); ++i) {
        const std::vector<float>& anchor = vertices[(i + 1) % children.size()];
        float even = 0.0f;
        float odd = 0.0f;
        const std::size_t pairs = point.size() / 2;
        for (std::size_t j = 0; j < pairs; ++j) {
            even = rs::mul_add(normals[i][j * 2], point[j * 2] - anchor[j * 2], even);
            odd = rs::mul_add(normals[i][j * 2 + 1], point[j * 2 + 1] - anchor[j * 2 + 1], odd);
        }
        float dot = even + odd;
        if (point.size() % 2 != 0) {
            const std::size_t j = point.size() - 1;
            dot += normals[i][j] * (point[j] - anchor[j]);
        }
        out.push_back(dot * scales[i]);
    }
    return out;
}

std::vector<float> Simplex::project(Slice<const float> point) const {
    std::vector<std::vector<float>> matrix = vertices;
    std::vector<float> projected;
    projected.reserve(point.size());
    for (float target : point) {
        const std::size_t count = matrix.size() - 1;
        std::vector<std::vector<float>> sliced;
        bool done = false;
        for (std::size_t i = 0; i < count && !done; ++i) {
            for (std::size_t j = i + 1; j < matrix.size(); ++j) {
                if (sliced.size() == count) {
                    done = true;
                    break;
                }
                const std::vector<float>& a = matrix[i];
                const std::vector<float>& b = matrix[j];
                if ((a[0] < target) == (b[0] < target)) {
                    continue;
                }
                const float delta = b[0] - a[0];
                if (rs::abs(delta) <= rs::from_bits(0x3727c5ac)) {
                    //8219B100
                    sliced.emplace_back(a.begin() + 1, a.end());
                    if (sliced.size() < count) {
                        sliced.emplace_back(b.begin() + 1, b.end());
                    }
                } else {
                    const float t = (target - a[0]) / delta;
                    std::vector<float> row;
                    const std::size_t n = std::min(a.size(), b.size());
                    for (std::size_t k = 1; k < n; ++k) row.push_back(rs::mul_add(b[k] - a[k], t, a[k]));
                    sliced.push_back(std::move(row));
                }
            }
        }
        if (sliced.empty()) {
            std::size_t nearest = 0;
            float distance = rs::f32::MAX;
            for (std::size_t i = 0; i < matrix.size(); ++i) {
                const std::vector<float>& row = matrix[i];
                const float d = (row[0] - target) * (row[0] - target);
                if (d < distance) {
                    distance = d;
                    nearest = i;
                }
            }
            projected.insert(projected.end(), matrix[nearest].begin(), matrix[nearest].end());
            break;
        }
        projected.push_back(target);
        while (sliced.size() < count) {
            sliced.push_back(sliced.back());
        }
        matrix = std::move(sliced);
    }
    return projected;
}

rs::Result<BlendSpace, std::string> BlendSpace::new_(std::vector<AttributeName> parameters,
                                                     std::vector<PlaybackTree> children,
                                                     std::vector<Simplex> simplexes) {
    const std::size_t d = parameters.size();
    auto invalid_simplex = [&](const Simplex& s) {
        if (s.children.size() != d + 1 || s.vertices.size() != d + 1 || s.normals.size() != d + 1 ||
            s.scales.size() != d + 1)
            return true;
        for (std::size_t i : s.children)
            if (i >= children.size()) return true;
        for (const auto* group : {&s.vertices, &s.normals})
            for (const auto& v : *group) {
                if (v.size() != d) return true;
                for (float x : v)
                    if (!rs::is_finite(x)) return true;
            }
        for (float v : s.scales)
            if (!rs::is_finite(v)) return true;
        return false;
    };
    bool invalid = d == 0 || d > 4 || children.size() < d + 1 || simplexes.empty();
    if (!invalid) {
        for (const Simplex& s : simplexes) {
            if (invalid_simplex(s)) {
                invalid = true;
                break;
            }
        }
    }
    if (invalid) {
        return rs::make_err(std::string("Invalid authored BlendSpace topology"));
    }
    std::vector<float> weights(d + 1, 0.0f);
    weights[0] = 1.0f;
    BlendSpace out{std::move(parameters), std::move(children), std::move(simplexes), std::vector<float>(d, 0.0f),
                   0, std::move(weights), 0.0f};
    return out;
}

std::vector<std::pair<std::size_t, float>> BlendSpace::active_weights() const {
    std::vector<std::pair<std::size_t, float>> out;
    const auto& indices = simplexes[current].children;
    for (std::size_t k = 0; k < indices.size() && k < weights.size(); ++k) out.emplace_back(indices[k], weights[k]);
    return out;
}

float BlendSpace::length() const {
    float sum = 0.0f;
    for (const auto& [i, w] : active_weights()) sum = rs::mul_add(w, children[i].length(), sum);
    return sum;
}

void BlendSpace::set_time(float time_) {
    const float phase = time_ / length();
    for (std::size_t i : simplexes[current].children) {
        PlaybackTree& child = children[i];
        child.set_time(phase * child.length());
    }
    //82D24AB0 updates child clocks; the cached time is written by Advance.
}

void BlendSpace::set_speed(float speed) {
    for (PlaybackTree& child : children) {
        child.set_speed(speed);
    }
}

void BlendSpace::advance(float dt, float phase, AdvanceResult& property) {
    const float length_ = length();
    const std::vector<std::size_t>& indices = simplexes[current].children;
    const std::size_t first = indices[0];
    const float first_length = children[first].length();
    for (std::size_t slot = 0; slot < indices.size(); ++slot) {
        PlaybackTree& child = children[indices[slot]];
        AdvanceResult result{false, -1.0f, -1.0f};
        child.advance((child.length() * (1.0f / length_)) * dt, phase, result);
        if (slot == 0) {
            property = result;
        }
    }
    const float scale = length_ / first_length;
    property.overshoot *= scale;
    property.remaining_before_wrap *= scale;
    time = children[first].time() * scale;
}

rs::Result<bool, std::string> BlendSpace::set_attributes(Slice<const SettableAttribute> attributes) {
    bool changed = false;
    for (const SettableAttribute& attribute : attributes) {
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (parameters[i] == attribute.name) {
                if (!rs::is_finite(attribute.value)) {
                    return rs::make_err(std::string("Nonfinite BlendSpace parameter"));
                }
                values[i] = attribute.value;
                changed = true;
                break;
            }
        }
    }
    if (!changed) {
        return false;
    }
    std::optional<std::pair<std::size_t, std::vector<float>>> selected;
    float closest = rs::f32::MAX;
    for (std::size_t index = 0; index < simplexes.size(); ++index) {
        const Simplex& simplex = simplexes[index];
        std::vector<float> w = simplex.coordinates(values);
        bool all_nonnegative = true;
        for (float x : w)
            if (!(x >= 0.0f)) all_nonnegative = false;
        if (all_nonnegative) {
            selected = std::make_pair(index, std::move(w));
            break;
        }
        w = simplex.coordinates(simplex.project(values));
        for (float& x : w) {
            if (x < 0.0f) {
                x = 0.0f;
            }
        }
        auto n = normalize(w);
        if (n.is_err()) return rs::make_err(n.unwrap_err());
        float distance = 0.0f;
        for (std::size_t j = 0; j < values.size(); ++j) {
            const float target = values[j];
            float projected = 0.0f;
            for (std::size_t k = 0; k < simplex.vertices.size() && k < w.size(); ++k)
                projected = rs::mul_add(simplex.vertices[k][j], w[k], projected);
            distance = rs::mul_add(projected - target, projected - target, distance);
        }
        if (distance < closest) {
            closest = distance;
            selected = std::make_pair(index, std::move(w));
        }
    }
    if (!selected) return rs::make_err(std::string("BlendSpace has no finite projection"));
    const std::size_t index = selected->first;
    std::vector<float> w = std::move(selected->second);
    for (float& x : w) {
        x = rs::clamp(x, 0.0f, 1.0f);
    }
    auto n = normalize(w);
    if (n.is_err()) return rs::make_err(n.unwrap_err());
    if (index != current) {
        const PlaybackTree& old = children[simplexes[current].children[0]];
        const float phase = old.time() / old.length();
        for (std::size_t i : simplexes[index].children) {
            PlaybackTree& child = children[i];
            child.set_time(phase * child.length());
        }
        current = index;
    }
    weights = std::move(w);
    return true;
}

rs::Result<std::vector<AnimationAttribute>, std::string> BlendSpace::attributes(std::uint32_t mask) const {
    const std::vector<std::size_t>& indices = simplexes[current].children;
    auto first = children[indices[0]].attributes(mask);
    if (first.is_err()) return first;
    std::vector<AnimationAttribute> output = std::move(first.unwrap());
    for (AnimationAttribute& a : output) {
        auto s = playback_attributes::scale(a, weights[0]);
        if (s.is_err()) return rs::make_err(s.unwrap_err());
    }
    for (std::size_t slot = 1; slot < indices.size(); ++slot) {
        auto rr = children[indices[slot]].attributes(mask);
        if (rr.is_err()) return rr;
        const std::vector<AnimationAttribute>& right = rr.unwrap();
        std::size_t next = 0;
        std::vector<AnimationAttribute> combined;
        for (AnimationAttribute& a : output) {
            while (next < right.size() && right[next].name._0 < a.name._0) {
                next += 1;
            }
            if (next < right.size() && right[next].name == a.name) {
                auto s = playback_attributes::add_weighted(a, right[next], weights[slot]);
                if (s.is_err()) return rs::make_err(s.unwrap_err());
                combined.push_back(a);
            }
        }
        output = std::move(combined);
    }
    return output;
}

rs::Result<bool, std::string> BlendSpace::query_attribute(AttributeName name, std::uint32_t mask,
                                                          AnimationAttribute& output) const {
    //82D24580 indexes the first d+1 authored children directly (unlike
    //GetAttributes, which follows the active simplex). Preserve that distinction.
    auto r0 = children[0].query_attribute(name, mask, output);
    if (r0.is_err()) return r0;
    if (!r0.unwrap()) {
        return false;
    }
    auto s = playback_attributes::scale(output, weights[0]);
    if (s.is_err()) return rs::make_err(s.unwrap_err());
    for (std::size_t i = 1; i < weights.size(); ++i) {
        AnimationAttribute right = MotionGraphAttribute{name, 0.0f}.to_animation();
        auto r = children[i].query_attribute(name, 15, right);
        if (r.is_err()) return r;
        if (!r.unwrap()) {
            return false;
        }
        auto a = playback_attributes::add_weighted(output, right, weights[i]);
        if (a.is_err()) return rs::make_err(a.unwrap_err());
    }
    return true;
}

rs::Result<bool, std::string> BlendSpace::evaluate(Evaluation parameters, bool enabled,
                                                   std::vector<PoseCommand>& output) {
    if (!enabled) {
        return false;
    }
    for (std::size_t i : simplexes[current].children) {
        auto r = children[i].evaluate(parameters, true, output);
        if (r.is_err()) return r;
        if (!r.unwrap()) {
            return rs::make_err(std::string("BlendSpace child produced no pose"));
        }
    }
    output.push_back(PoseCommand::WeightedBlend{weights});
    return true;
}

rs::Result<rs::Unit, std::string> normalize(Slice<float> weights) {
    const float sum = rs::sum(weights);
    if (!rs::is_finite(sum) || sum <= 0.0f) {
        return rs::make_err(std::string("Degenerate BlendSpace weights"));
    }
    const float inverse = 1.0f / sum;
    for (float& w : weights) {
        w *= inverse;
    }
    return rs::Unit{};
}

}  // namespace s3::animation::playback_tree::blend_space
