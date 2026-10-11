// Port of skate-core/src/player/offboard/grab_scene/qualify/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/grab_scene.hpp"
#include "s3/core/player/offboard/grab_scene/qualify/math.hpp"
#include "s3/core/player/offboard/grab_scene/qualify/polyline.hpp"

namespace {
using namespace s3::player::offboard::grab_scene;
namespace polyline = s3::player::offboard::grab_scene::qualify_::polyline;
using s3::player::offboard::grab_scene::qualify_::math::length;
using s3::player::offboard::grab_scene::qualify_::math::sub;

Record spline(std::vector<Vector> points) {
    auto g = std::make_shared<Geometry>();
    g->id = 1;
    g->points = std::move(points);
    g->approach_vectors = {Vector{0.0f, 0.0f, 1.0f, 0.0f}};
    g->word_60 = 0;
    Record r{{}, g};
    const Vector axes[4] = {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, Vector{}};
    for (std::size_t i = 0; i < 4; ++i) r.set_vector(i * 16, axes[i]);
    const Vector a = r.points().front();
    const Vector b = r.points().back();
    r.set_vector(64, a);
    r.set_vector(80, b);
    r.set_vector(96, {0.0f, 0.0f, 1.0f, 0.0f});
    r._0[49] = 1;
    float total = -0.0f;  // Rust iterator sum starts at -0.0
    for (std::size_t k = 0; k + 1 < r.points().size(); ++k) total += length(sub(r.points()[k + 1], r.points()[k]));
    r._0[44] = s3::rs::to_bits(total);
    return r;
}

S3_TEST(evaluates_full_polyline_not_endpoint_chord) {
    const auto r = spline({Vector{}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 0.0f}});
    const auto point = polyline::at_distance(r, 1.5f);
    CHECK(std::fabs(point[0] - 1.0f) < 1e-6f && std::fabs(point[2] - 0.5f) < 1e-6f);
    const float along = polyline::nearest_distance(r, {1.0f, 0.0f, 0.5f, 0.0f});
    CHECK(std::fabs(along - 1.5f) < 1e-6f);
}

S3_TEST(reverse_flag_changes_arc_length_traversal) {
    auto r = spline({Vector{}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 0.0f}});
    r._0[50] |= 0x20000000u;
    const auto point = polyline::at_distance(r, 0.5f);
    CHECK(std::fabs(point[0] - 1.0f) < 1e-6f && std::fabs(point[2] - 0.5f) < 1e-6f);
}

S3_TEST(qualification_requires_real_approach_facing_and_box) {
    const auto r = spline({{-1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}});
    const Bounds bounds{{{{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 0.0f}, Vector{}}},
                        {2.0f, 1.0f, 2.0f, 0.0f}};
    const BoardLimits limits{0.1f, 1.0f, 1.0f};
    CHECK(qualify(r, {0.0f, 0.0f, 1.0f, 0.0f}, bounds, limits));
    auto away = bounds;
    away.frame[2] = {0.0f, 0.0f, 1.0f, 0.0f};
    CHECK(!qualify(r, {0.0f, 0.0f, 1.0f, 0.0f}, away, limits));
    auto remote = bounds;
    remote.frame[3] = {5.0f, 0.0f, 0.0f, 0.0f};
    CHECK(!qualify(r, {0.0f, 0.0f, 1.0f, 0.0f}, remote, limits));
}
}  // namespace
