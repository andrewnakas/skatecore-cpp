// Port of skate-core/src/scoring/catalog.rs (#[cfg(test)] mod tests).
#include <string_view>

#include "harness.hpp"
#include "s3/core/scoring/catalog.hpp"

namespace {
using namespace s3::scoring;
using namespace s3::scoring::catalog;

/// The five air metrics, the gap/context total and the off-board total are credited
/// by bare id at 82DA8550 and 82DAB6A8. None of them has an authored record, so a
/// lookup that requires one drops every one of their rewards -- which is precisely
/// what made air scores a fraction of retail's. They must resolve here.
S3_TEST(the_metric_scorables_resolve_without_an_authored_record) {
    const std::pair<std::size_t, std::string_view> cases[7] = {
        {129, "air_horizontal_distance"}, {130, "air_height_to_peak"}, {131, "air_total_height_gain"},
        {132, "air_player_spin"},         {133, "air_player_flip"},    {237, "air_metric"},
        {253, "generic_metric"},
    };
    for (const auto& [id, name] : cases) {
        const auto found = metadata(id);
        CHECK_MSG(found.has_value(), name << " (" << id << ") has no metadata");
        const Scorable metric = *found;
        CHECK_MSG(std::get<0>(IDENTIFIERS[id]) == name, "catalog order moved under " << id);
        // Class 5 keeps them out of the repetition penalty and out of the flip
        // bucket, and score type 0 keeps them out of the sequence histories.
        CHECK_MSG(metric.class_ == 5u, name);
        CHECK_MSG(metric.score_type == 0u, name);
        CHECK_MSG(metric.valid(), name);
        CHECK_MSG(!metric.repetition_applies(), name);
    }
    CHECK(metadata(IDENTIFIERS.size()) == std::nullopt);
}
}  // namespace
