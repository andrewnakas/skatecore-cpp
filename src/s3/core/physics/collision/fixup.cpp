// Port of skate-core/src/physics/collision/fixup.rs.
#include "s3/core/physics/collision/fixup.hpp"

#include <cmath>
#include <optional>

#include "s3/core/physics/collision/arithmetic.hpp"
#include "s3/core/physics/collision/edge.hpp"

namespace s3::physics::collision::fixup {

using namespace arithmetic;
using edge::edge_cos_test;

TriangleRegion TriangleFeature::classify(Vector3 direction) const {
    using R = TriangleRegion;
    const float a = dot(direction, edges[0]);
    const float b = dot(direction, edges[1]);
    const float c = dot(direction, edges[2]);
    const float t = rs::from_bits(0x3D4CCCCDu);
    if (a > t && b < -t) {
        return R::Vertex2;
    } else if (b > t && c < -t) {
        return R::Vertex1;
    } else if (c > t && a < -t) {
        return R::Vertex0;
    } else if (a > -c && -b >= a) {
        return R::Edge2;
    } else if (b > -a && -c >= b) {
        return R::Edge1;
    } else if (c > -b && -a >= c) {
        return R::Edge0;
    }
    return R::Face;
}

namespace {

void bend(Vector3& normal, ContactPair* contacts, std::size_t count, Vector3 direction, bool reverse) {
    normal = reverse ? neg(direction) : direction;
    for (std::size_t k = 0; k < count; ++k) {
        ContactPair& pair = contacts[k];
        if (reverse) {
            pair.b = madd(direction, dot(sub(pair.b, pair.a), direction), pair.a);
        } else {
            pair.a = madd(direction, dot(sub(pair.a, pair.b), direction), pair.b);
        }
    }
}

double f64_from_bits(std::uint64_t bits) {
    double d;
    std::memcpy(&d, &bits, sizeof d);
    return d;
}

bool one_sided_edge(TriangleFeature t, std::size_t edge, float projection, Vector3& normal,
                    ContactPair* contacts, std::size_t count, TriangleFixup s) {
    const float cosine = t.edge_cosines[edge];
    // Scalar fadds, not an epsilon comparison on a vector dot product.
    if (t.edge_convex(edge) ||
        (s.is_object && std::fabs(static_cast<double>(cosine - 1.0f)) < f64_from_bits(0x3EE4F8B588E368F1ull))) {
        return projection + s.convexity_epsilon >= cosine;
    }
    if (cosine > s.edge_cos_bend_normal_threshold || cosine <= rs::from_bits(0x3A83126Fu)) {
        return false;
    }
    const float squared = 1.0f - cosine * cosine;
    const float threshold = squared == 0.0f ? 0.0f : squared * inverse_length_squared(squared, 2);
    if (projection > threshold) {
        bend(normal, contacts, count, t.normal, s.reverse);
        return true;
    }
    return false;
}

}  // namespace

bool fix_up_triangle(TriangleFeature triangle, Vector3& normal, ContactPair* contacts,
                     std::size_t count, TriangleFixup settings) {
    S3_ASSERT(count <= 16);
    using R = TriangleRegion;
    const Vector3 toward = settings.reverse ? neg(normal) : normal;
    const float projection = dot(toward, triangle.normal);
    const float face_tolerance = rs::from_bits(0x3F7FF62Bu);
    const R region = rs::abs(projection) < face_tolerance
                         ? triangle.classify(normalize(sub(toward, scale(triangle.normal, projection)), 2))
                         : R::Face;
    const bool one_sided = (triangle.flags & TriangleFeature::ONE_SIDED) != 0;
    std::optional<std::size_t> edge;
    switch (region) {
        case R::Edge0: edge = 0; break;
        case R::Edge1: edge = 1; break;
        case R::Edge2: edge = 2; break;
        default: break;
    }
    std::optional<std::size_t> vertex;
    switch (region) {
        case R::Vertex0: vertex = 0; break;
        case R::Vertex1: vertex = 1; break;
        case R::Vertex2: vertex = 2; break;
        default: break;
    }
    if ((triangle.flags & TriangleFeature::USE_EDGE_COSINES) == 0) {
        if (region == R::Face) {
            return !one_sided || projection > 0.0f;
        }
        if (one_sided) {
            if (projection > face_tolerance) {
                return true;
            }
            if (projection < 0.0f) {
                return false;
            }
        } else if (rs::abs(projection) > face_tolerance) {
            return true;
        }
        if (edge) return triangle.edge_convex(*edge);
        return !triangle.vertex_disabled(rs::unwrap(vertex));
    }
    if (region == R::Face) {
        return !one_sided || projection > 0.0f;
    }
    if (edge) {
        const std::size_t i = *edge;
        if (!one_sided) {
            return (triangle.edge_convex(i) ? projection : -projection) >= triangle.edge_cosines[i];
        }
        return one_sided_edge(triangle, i, projection, normal, contacts, count, settings);
    }
    const std::size_t v = rs::unwrap(vertex);
    // Native vertex adjacency: (edge2,edge0), (edge0,edge1), (edge1,edge2).
    const std::size_t first = (v + 2) % 3;
    const std::size_t second = v;
    const std::size_t pair[2] = {first, second};
    if (!one_sided) {
        if (triangle.vertex_disabled(v)) return false;
        for (std::size_t i : pair) {
            if (!edge_cos_test(neg(triangle.edges[2 - i]), triangle.normal, toward,
                               triangle.edge_cosines[i], triangle.edge_convex(i), true)) {
                return false;
            }
        }
        return true;
    }
    const bool disabled = triangle.vertex_disabled(v) && !settings.is_object;
    if (projection > 0.0f && disabled) {
        // TU3-only recovery for a disabled vertex bordering exactly one convex
        // edge. Skate 2's corresponding routine does not have this path.
        std::size_t e;
        const bool cf = triangle.edge_convex(first), cs = triangle.edge_convex(second);
        if (cf && !cs) {
            e = second;
        } else if (!cf && cs) {
            e = first;
        } else {
            return false;
        }
        const float cosine = triangle.edge_cosines[e];
        if (!(cosine <= rs::from_bits(0x3F7851ECu) && cosine > rs::from_bits(0x3A83126Fu))) {
            return false;
        }
        const Vector3 direction = triangle.edges[2 - (e == second ? first : second)];
        const Vector3 bent = normalize(cross(cross(direction, toward), direction), 1);
        bend(normal, contacts, count, bent, settings.reverse);
        return true;
    }
    if (disabled) {
        return false;
    }
    for (std::size_t i : pair) {
        if (!edge_cos_test(neg(triangle.edges[2 - i]), triangle.normal, toward, triangle.edge_cosines[i],
                           triangle.edge_convex(i), false)) {
            return false;
        }
    }
    return true;
}

}  // namespace s3::physics::collision::fixup
