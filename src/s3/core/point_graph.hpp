// Port of skate-core/src/point_graph.rs: TU3 `PointGraph` scalar evaluator, `0x82481E10`.
#pragma once
#include <array>
#include <cstddef>

#include "s3/core/rs.hpp"

namespace s3::point_graph {

template <std::size_t N>
struct PointGraph {
    std::array<float, N> x{};
    std::array<float, N> y{};

    /// Native endpoint handling and slope-first binary32 interpolation.
    float evaluate(float input) const {
        static_assert(N > 0, "PointGraph needs at least one key");
        if (input < x[0]) {
            return y[0];
        }
        // The native branch enters the scan only on an ordered less-than.
        // Unordered input or a NaN final key therefore selects the last value.
        if (!(input < x[N - 1])) {
            return y[N - 1];
        }
        for (std::size_t upper = 1; upper < N; ++upper) {
            if (!(input < x[upper])) {
                continue;
            }
            const std::size_t lower = upper - 1;
            const float width = x[upper] - x[lower];
            if (width <= 0.0f) {
                return y[upper];
            }
            const float slope = (y[upper] - y[lower]) / width;
            return rs::mul_add(slope, input - x[lower], y[lower]);
        }
        return y[0];
    }
};
template <std::size_t N>
inline bool operator==(const PointGraph<N>& a, const PointGraph<N>& b) {
    return a.x == b.x && a.y == b.y;
}
template <std::size_t N>
inline bool operator!=(const PointGraph<N>& a, const PointGraph<N>& b) {
    return !(a == b);
}

}  // namespace s3::point_graph
