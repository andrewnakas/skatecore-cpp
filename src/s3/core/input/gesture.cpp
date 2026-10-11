// Port of skate-core/src/input/gesture.rs.
// TU3 pattern recognition, 82697168/826972B8/826974A8.
#include "s3/core/input/gesture.hpp"

#include <algorithm>
#include <utility>

namespace s3::input::gesture {

namespace {
float distance_squared(std::array<float, 2> a, std::array<float, 2> b) {
    const float x = a[0] - b[0];
    const float y = a[1] - b[1];
    // VMX multiplies then adds; do not contract to FMA.
    return x * x + y * y;
}
}  // namespace

void Node::tick(const Pattern& pattern, std::array<float, 2> sample, std::uint8_t maximum_misses) {
    if (!active) {
        const float d = distance_squared(pattern.points.at(0), sample);
        if (d <= pattern.tolerance_squared) {
            *this = Node{};
            active = true;
            next = 1;
            elapsed = 1;
            distance = d;
        }
    } else if (!complete) {
        const float d = distance_squared(pattern.points.at(next), sample);
        if (d <= pattern.tolerance_squared) {
            distance += d;
            elapsed = std::uint16_t((elapsed + 1) & 0x3ff);
            if (next + 1 == pattern.points.size()) {
                complete = true;
            } else {
                next += 1;
                misses = 0;
            }
        } else {
            // Holding the first point does not age a pending gesture.
            if (next != 1 || distance_squared(pattern.points.at(0), sample) > pattern.tolerance_squared) {
                elapsed = std::uint16_t((elapsed + 1) & 0x3ff);
                misses = std::uint8_t((misses + 1) & 0x3f);
            }
            if (misses > maximum_misses) {
                *this = Node{};
            }
        }
    }
}

float Node::score(std::size_t count_) const {
    const float count = float(count_);
    // The unusual two-sided 0.15 operation is present in 826972B8.
    const float mean = rs::min(rs::max(distance, 0.15f) / count, 0.15f);
    return (count * count * count * count) / (mean * float(elapsed));
}

rs::Result<Recognizer, std::string> Recognizer::new_(std::vector<Pattern> patterns) {
    for (const Pattern& pattern : patterns) {
        bool bad_point = false;
        for (const auto& p : pattern.points) {
            for (float v : p) {
                if (!rs::is_finite(v)) bad_point = true;
            }
        }
        const std::size_t len = pattern.points.size();
        if (!(len >= 2 && len <= 15) || !rs::is_finite(pattern.tolerance_squared) ||
            pattern.tolerance_squared < 0.0f || bad_point) {
            return rs::make_err(std::string("invalid native gesture pattern ") + pattern.name);
        }
    }
    Recognizer r;
    r.nodes_ = std::vector<Node>(patterns.size(), Node{});
    r.patterns_ = std::move(patterns);
    return r;
}

std::optional<std::size_t> Recognizer::held(std::array<float, 2> sample) {
    if (held_) {
        const Pattern& pattern = patterns_.at(*held_);
        if (distance_squared(pattern.points.back(), sample) > pattern.tolerance_squared) {
            held_ = std::nullopt;
        }
    }
    return held_;
}

std::optional<Recognition> Recognizer::sample(std::array<float, 2> sample, Settings settings) {
    if (refractory_) {
        std::fill(nodes_.begin(), nodes_.end(), Node{});
        refractory_ = false;
        return std::nullopt;
    }
    if (!has_previous_sample_) {
        has_previous_sample_ = true;
        return std::nullopt;
    }
    std::optional<std::size_t> best;
    for (std::size_t i = 0; i < nodes_.size() && i < patterns_.size(); ++i) {
        nodes_[i].tick(patterns_[i], sample, settings.maximum_misses);
    }
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        const Node& node = nodes_[index];
        if (node.complete &&
            (!best || node.score(patterns_[index].points.size()) >
                          nodes_[*best].score(patterns_[*best].points.size()))) {
            best = index;
        }
    }
    if (!best) {
        return std::nullopt;
    }
    const std::size_t pattern = *best;
    const Node node = nodes_[pattern];
    const float ratio = float(node.elapsed) / float(patterns_[pattern].points.size());
    // 822249B4/82063B08 and 821E63E8/821E63EC, checked against TU3.
    float low, high;
    if (settings.difficulty == 2) {
        low = 1.5f;
        high = 3.0f;
    } else {
        low = 1.75f;
        high = 4.4f;
    }
    float strength;
    if (ratio <= low) {
        strength = 1.0f;
    } else if (ratio >= high) {
        strength = 0.0f;
    } else {
        const float slope = 1.0f / (low - high);
        strength = rs::mul_add(slope, ratio, -(slope * high));
    }
    refractory_ = true;
    held_ = pattern;
    return Recognition{pattern, strength, node.distance, float(node.elapsed)};
}

}  // namespace s3::input::gesture
