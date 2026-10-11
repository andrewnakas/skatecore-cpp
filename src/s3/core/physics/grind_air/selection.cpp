// Port of skate-core/src/physics/grind_air/selection.rs.
#include "s3/core/physics/grind_air.hpp"

namespace s3::physics::grind_air::selection {

Contacts contacts(const prediction::Samples& samples, std::size_t count, V origin, V rail, V normal) {
    Contacts out{
        {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f},
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {ZERO, ZERO, ZERO, ZERO, ZERO},
        static_cast<float>(count),
    };
    for (std::size_t p = 0; p < 5; ++p) {
        for (std::size_t i = 1; i < count; ++i) {
            S3_ASSERT(i < samples.size());
            const float a = dot(normal, sub(samples[i - 1][p], origin));
            const float b = dot(normal, sub(samples[i][p], origin));
            if (a * b < 0.0f) {
                const float f = rs::abs(a / (a - b));
                const V at = madd(samples[i - 1][p], 1.0f - f, scale(samples[i][p], f));
                const V on = madd(rail, dot(sub(at, origin), rail), origin);
                out.times[p] = static_cast<float>(i - 1) + f;
                out.corrections[p] = sub(on, at);
                out.distances[p] = length(out.corrections[p]);
                out.earliest = rs::min(out.earliest, out.times[p]);
                break;
            }
        }
    }
    return out;
}

std::optional<std::size_t> choose(const Contacts& c, const std::array<float, 12>& headings,
                                  const Settings& s, bool inverted,
                                  std::optional<std::size_t> previous) {
    auto valid = [&](std::size_t kind) {
        const std::size_t p = targets::CONTACTS[kind];
        const float t = c.times[p];
        if (kind == 4) {
            if (rs::min(c.times[2], c.times[3]) < 4.0f && rs::abs(c.times[2] - c.times[3]) > 2.0f) {
                return false;
            }
        } else if (c.earliest < 5.0f && rs::abs(c.earliest - t) > 3.0f) {
            return false;
        }
        if (!(t > 0.0f && c.distances[p] < s.distances[kind])) {
            return false;
        }
        const std::size_t index = rs::as_usize(t + 0.5f);
        S3_ASSERT(index < headings.size());
        return rs::abs(targets::DESIRED[kind] - rs::abs(headings[index])) < s.ranges[kind];
    };
    std::optional<std::size_t> selected;
    for (std::size_t k = 0; k < 7; ++k) {
        std::size_t kind = k;
        if (inverted != (kind == 0) || !valid(kind)) {
            continue;
        }
        if (kind == 3) {
            if (previous && (*previous == 1 || *previous == 2)) {
                const std::size_t old = *previous;
                if (valid(old)) {
                    kind = old;
                }
            }
        }
        if (!selected ||
            c.distances[targets::CONTACTS[kind]] < c.distances[targets::CONTACTS[*selected]]) {
            selected = kind;
        }
        if (selected && *selected < 5) {
            break;
        }
    }
    return selected;
}

}  // namespace s3::physics::grind_air::selection
