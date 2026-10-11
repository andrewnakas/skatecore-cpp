// Port of skate-core/src/player/offboard/air_selector/ledge.rs (whole file).
// Air-only ledge correction82D6D4D0, filtering82C1F310, six-line82C20728.
// Candidate0 only. Closest-point/plane formulas read from82E09D28/09DF0.
#pragma once
#include <array>
#include <optional>
#include <vector>

#include "s3/core/player/offboard/air_selector/math.hpp"
#include "s3/core/player/offboard/biped_air/selection.hpp"
#include "s3/core/player/offboard/ground_query.hpp"

namespace s3::player::offboard::air_selector {
struct Context;
}

namespace s3::player::offboard::air_selector::ledge {

using air::trajectory::Prediction;
using air::trajectory::Trajectory;
using biped_air::recovered::selection::Candidate;
using ground_query::Edge;
using ground_query::EdgeSearch;
using ground_query::Line;
using ground_query::LineHit;
using Vector = std::array<float, 4>;

std::optional<EdgeSearch> search(const Candidate& candidate, const Prediction& prediction, const Context& context);
///82C1F310 removes a parallel edge only with native separation, overlap,
/// reference-distance and oriented-height tests. Retains provider order.
std::vector<Edge> filter_edges(const std::vector<Edge>& edges, Vector reference);
///82D60C80/B98: exact discriminant branches, larger root for two roots.
std::optional<float> plane_time(Trajectory t, Vector point, Vector normal);

struct Adjustment {
    Edge edge{};
    Vector point{};
    Trajectory lowered_trajectory{};
    std::int32_t landing_frame = 0;
    float radius = 0.0f;

    void apply(Candidate& candidate) const;
};

std::optional<Adjustment> choose(const Candidate& candidate, const Prediction& prediction, const Context& context,
                                 float radius, const std::vector<Edge>& edges);
///82C20530 clears byte64: Air does not append Ground's downward seventh line.
std::optional<std::array<Line, 6>> lines(const Adjustment& adjustment, float half_wheelbase);
///82C20C08: return1 after consuming a submitted batch, even with six misses.
rs::Result<rs::Unit, const char*> consume_lines(const std::vector<std::optional<LineHit>>& hits);

}  // namespace s3::player::offboard::air_selector::ledge
