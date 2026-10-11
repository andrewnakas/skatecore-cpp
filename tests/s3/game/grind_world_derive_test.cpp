// Port of the #[cfg(test)] mod tests in skate-game/src/grind_world/derive.rs.
// (`supplied_triangle_dump` is #[ignore]d in Rust and needs an external dump; not ported.)
#include <cmath>

#include "harness.hpp"
#include "s3/game/grind_world/derive.hpp"

using namespace s3::game::grind_world::derive;

namespace {

using T = std::array<std::array<float, 3>, 3>;
using Line = std::vector<std::array<float, 3>>;

void quad(std::vector<T>& out, std::array<std::array<float, 3>, 4> q) {
    out.push_back({q[0], q[1], q[2]});
    out.push_back({q[0], q[2], q[3]});
}
void floor_(std::vector<T>& out, float half) {
    quad(out, {{{-half, 0.0f, -half}, {-half, 0.0f, half}, {half, 0.0f, half}, {half, 0.0f, -half}}});
}
/// Closed box, faces wound outward.
void solid(std::vector<T>& out, std::array<float, 3> min, std::array<float, 3> max) {
    const float x0 = min[0], y0 = min[1], z0 = min[2];
    const float x1 = max[0], y1 = max[1], z1 = max[2];
    quad(out, {{{x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}}});
    quad(out, {{{x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}}});
    quad(out, {{{x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}}});
    quad(out, {{{x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}}});
    quad(out, {{{x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0}}});
    quad(out, {{{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}}});
}
std::vector<Line> found(const std::vector<T>& t) { return rails(t, {}).rails; }
float span(const Line& line) {
    float total = -0.0f;
    for (std::size_t k = 0; k + 1 < line.size(); ++k) {
        float s = -0.0f;
        for (std::size_t i = 0; i < 3; ++i) s += s3::rs::powi(line[k + 1][i] - line[k][i], 2);
        total += std::sqrt(s);
    }
    return total;
}

}  // namespace

S3_TEST(flat_ground_and_gentle_banks_have_no_edges) {
    std::vector<T> t;
    floor_(t, 10.0f);
    // 30 degree bank up to a deck.
    quad(t, {{{0.0f, 0.0f, -2.0f}, {0.0f, 0.0f, 2.0f}, {1.732f, 1.0f, 2.0f}, {1.732f, 1.0f, -2.0f}}});
    quad(t, {{{1.732f, 1.0f, -2.0f}, {1.732f, 1.0f, 2.0f}, {5.0f, 1.0f, 2.0f}, {5.0f, 1.0f, -2.0f}}});
    // Open sides and the deck's back drop to the floor; the creases across the bank do not count.
    const auto f = found(t);
    CHECK(!f.empty());
    for (const auto& l : f)
        for (const auto& p : l) CHECK(p[0] > 4.99f || std::fabs(p[2]) > 1.99f);
}

S3_TEST(ledge_box_gives_its_four_top_edges) {
    std::vector<T> t;
    floor_(t, 10.0f);
    solid(t, {-2.0f, 0.0f, -0.5f}, {2.0f, 0.5f, 0.5f});
    const auto f = found(t);
    CHECK_EQ(f.size(), std::size_t(4));
    for (const auto& l : f) {
        CHECK_EQ(l.size(), std::size_t(2));
        for (const auto& p : l) CHECK(p[1] == 0.5f);
    }
    int four = 0, one = 0;
    for (const auto& l : f) {
        if (std::fabs(span(l) - 4.0f) < 1e-4f) ++four;
        if (std::fabs(span(l) - 1.0f) < 1e-4f) ++one;
    }
    CHECK_EQ(four, 2);
    CHECK_EQ(one, 2);
}

S3_TEST(stair_noses_are_not_rails_but_the_drop_behind_is) {
    std::vector<T> t;
    floor_(t, 10.0f);
    for (int step = 0; step < 3; ++step) {
        const float s = float(step);
        solid(t, {-1.0f, 0.0f, s * 0.3f}, {1.0f, 0.15f * (s + 1.0f), s * 0.3f + 0.3f});
    }
    const auto f = found(t);
    CHECK_EQ(f.size(), std::size_t(1));
    for (const auto& p : f[0]) CHECK(std::fabs(p[1] - 0.45f) < 1e-5f && std::fabs(p[2] - 0.9f) < 1e-5f);
}

S3_TEST(thin_bar_becomes_one_centred_rail) {
    std::vector<T> t;
    floor_(t, 10.0f);
    solid(t, {-2.0f, 0.35f, -0.05f}, {2.0f, 0.45f, 0.05f});
    const auto f = found(t);
    CHECK_EQ(f.size(), std::size_t(1));
    CHECK(std::fabs(span(f[0]) - 4.0f) < 1e-4f);
    for (const auto& p : f[0]) CHECK(std::fabs(p[2]) < 1e-5f && std::fabs(p[1] - 0.45f) < 1e-5f);
}

S3_TEST(edges_that_already_have_a_rail_are_left_out) {
    std::vector<T> t;
    floor_(t, 10.0f);
    solid(t, {-2.0f, 0.35f, -0.05f}, {2.0f, 0.45f, 0.05f});
    const auto derived = rails(t, {Line{{-2.0f, 0.45f, 0.0f}, {2.0f, 0.45f, 0.0f}}});
    CHECK((derived.own_blocked == std::vector<bool>{false}));
    CHECK(derived.rails.empty());
}

S3_TEST(wide_bar_keeps_both_edges_and_rejects_a_centre_rail) {
    std::vector<T> t;
    floor_(t, 10.0f);
    solid(t, {-2.0f, 0.35f, -0.15f}, {2.0f, 0.45f, 0.15f});
    const auto derived = rails(t, {Line{{-2.0f, 0.45f, 0.0f}, {2.0f, 0.45f, 0.0f}}});
    CHECK((derived.own_blocked == std::vector<bool>{true}));
    std::vector<Line> long_;
    for (const auto& l : derived.rails)
        if (span(l) > 3.9f) long_.push_back(l);
    CHECK_EQ(long_.size(), std::size_t(2));
    for (const auto& l : long_)
        for (const auto& p : l) CHECK(std::fabs(std::fabs(p[2]) - 0.15f) < 1e-5f);
}

S3_TEST(winding_does_not_matter) {
    std::vector<T> t;
    floor_(t, 10.0f);
    solid(t, {-2.0f, 0.0f, -0.5f}, {2.0f, 0.5f, 0.5f});
    for (auto& tri : t) std::swap(tri[1], tri[2]);
    CHECK_EQ(found(t).size(), std::size_t(4));
}
