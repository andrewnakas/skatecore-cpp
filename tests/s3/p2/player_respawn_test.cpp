// Port of skate-core/src/player/tests/respawn.rs.
#include <string>
#include <vector>

#include "harness.hpp"
#include "s3/core/player/respawn.hpp"

namespace {
using namespace s3::player::respawn;
using s3::rs::Result;
using s3::rs::Unit;

struct Scene {
    using Error = Unit;
    bool current = false;
    std::optional<float> rejected_x;
    std::vector<std::string> calls;
    Result<std::optional<Ground>, Unit> ground(const Matrix&) {
        calls.push_back("ground");
        if (current) return std::optional<Ground>(Ground{{8.f, 2.f, 9.f, 0.f}, true});
        return std::optional<Ground>();
    }
    Result<bool, Unit> location(const Matrix& m) {
        calls.push_back("location");
        return !(rejected_x && *rejected_x == m[3][0]);
    }
    Result<bool, Unit> occupants(const Matrix&) {
        calls.push_back("occupants");
        return true;
    }
    Result<bool, Unit> edges(const Matrix&) {
        calls.push_back("edges");
        return true;
    }
};
Candidate candidate(float x, float score) {
    Matrix transform = IDENTITY;
    transform[3][0] = x;
    return Candidate{transform, 1, false, score};
}
using Calls = std::vector<std::string>;

S3_TEST(scored_selection_consumes_entries_and_does_not_recheck_terrain) {
    History h = History::new_(candidate(-10.f, 0.f));
    h.insert(candidate(10.f, 100.f));
    h.insert(candidate(20.f, 100.f));
    Scene s;
    CHECK_EQ(h.automatic(0, s).unwrap().transform[3][0], 10.f);
    CHECK((s.calls == Calls{"ground", "location", "occupants"}));
    CHECK_EQ(h.automatic(0, s).unwrap().transform[3][0], 20.f);
    CHECK_EQ(h.automatic(0, s).unwrap().transform[3][0], -10.f);
    CHECK_EQ(h.automatic(0, s).unwrap().transform[3][0], -10.f);
}

S3_TEST(rejection_removes_candidate_and_recomputes_age) {
    History h = History::new_(candidate(-10.f, 0.f));
    h.insert(candidate(10.f, 100.f));
    h.insert(candidate(20.f, 50.f));
    Scene s;
    s.rejected_x = 10.f;
    CHECK_EQ(h.automatic(1, s).unwrap().transform[3][0], -10.f);
    CHECK_EQ(h.entries.size(), std::size_t(1));
    CHECK_EQ(h.entries[0].transform[3][0], 20.f);
}

S3_TEST(current_candidate_keeps_heading_and_horizontal_position_and_history) {
    History h = History::new_(candidate(-10.f, 0.f));
    h.current_position = {3.f, 4.f, 5.f, 0.f};
    h.current_orientation[2] = {1.f, 0.f, 0.f, 0.f};
    Scene s;
    s.current = true;
    const Candidate c = h.automatic(7, s).unwrap();
    CHECK_EQ(c.transform[3], (std::array<float, 4>{3.f, 2.f, 5.f, 0.f}));
    CHECK_EQ(c.transform[2], (std::array<float, 4>{1.f, 0.f, 0.f, 0.f}));
    CHECK_EQ(c.stance, 7u);
    CHECK(c.offboard);
    CHECK_EQ(h.entries.size(), std::size_t(1));
    CHECK((s.calls == Calls{"ground", "location", "occupants", "edges"}));
}

S3_TEST(insertion_capacity_and_native_gate_boundaries) {
    History h = History::new_(candidate(0.f, 0.f));
    CHECK(!h.recording_due(119, {10.f, 0.f, 0.f, 0.f}));
    CHECK(h.recording_due(120, {1.5f, 0.f, 0.f, 0.f}));
    CHECK(!h.recording_due(120, {1.499f, 0.f, 0.f, 0.f}));
    h.insert(candidate(10.f, 0.f));
    for (int i = 0; i < 20; ++i) {
        CHECK(!h.recording_due(120, {1000.f, 0.f, 0.f, 0.f}));
    }
    CHECK(h.recording_due(120, {1000.f, 0.f, 0.f, 0.f}));
    for (int n = 0; n < 40; ++n) {
        h.insert(candidate(static_cast<float>(n), 0.f));
    }
    CHECK_EQ(h.entries.size(), std::size_t(32));
    CHECK_EQ(h.entries[0].transform[3][0], 8.f);
    CHECK_EQ(h.initial.transform[3][0], 0.f);
}

S3_TEST(category_dispatch_matches_decrementing_ppc_branch_table) {
    std::vector<std::uint32_t> accepted;
    for (std::uint32_t x = 0; x <= 15; ++x)
        if (surface_allowed(x)) accepted.push_back(x);
    CHECK((accepted == std::vector<std::uint32_t>{0, 1, 2, 3, 4, 7, 8, 10, 11, 14, 15}));
    std::vector<float> scores;
    for (std::uint32_t x = 0; x <= 12; ++x) scores.push_back(surface_score(x));
    CHECK((scores == std::vector<float>{0.f, 100.f, 100.f, 50.f, 50.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 20.f, 0.f}));
}

S3_TEST(exhausted_history_fallback_uses_current_actor_stance) {
    History h = History::new_(candidate(-10.f, 100.f));
    Scene s;
    s.rejected_x = -10.f;
    const Candidate c = h.automatic(0, s).unwrap();
    CHECK(c.transform == h.initial.transform);
    CHECK_EQ(c.stance, 0u);
    CHECK(!c.offboard);
    CHECK_EQ(c.score, 0.f);
    CHECK(h.entries.empty());
}
}  // namespace
