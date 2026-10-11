// Port of skate-core/src/physics/triangle_sweep_walk.rs.
#include "s3/core/physics/triangle_sweep_walk.hpp"

#include <optional>

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/triangle_sweep.hpp"
#include "s3/core/physics/triangle_sweep_roots.hpp"

namespace s3::physics::triangle_sweep_walk {

using math::Vector3;
using triangle_query::dot;
using triangle_query::madd;
using triangle_query::scale;
using triangle_query::sub;
using triangle_query::TriangleLineHit;
using triangle_sweep::add;
using triangle_sweep::MIN_RECIPROCAL;
using triangle_sweep_roots::Fraction;
using triangle_sweep_roots::Root;
namespace roots = triangle_sweep_roots;

namespace {
bool replaces(std::optional<Fraction> hit, Fraction candidate) {
    return candidate.denominator > MIN_RECIPROCAL && candidate.numerator > 0.0f &&
           (!hit.has_value() || !hit->before(candidate));
}
void advance_fraction(TriangleLineHit& result, float fraction) {
    result.fraction = rs::mul_add(1.0f - result.fraction, fraction, result.fraction);
}
float inverse_radius(float radius) {
    float inverse = native_arithmetic::reciprocal_estimate(radius);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse, rs::mul_add(-inverse, radius, 1.0f), inverse);
    }
    return inverse;
}
std::optional<Fraction> root_hit(const Root& root, bool& away) {
    away = false;
    switch (root.kind) {
        case Root::Kind::Away:
            away = true;
            return std::nullopt;
        case Root::Kind::Miss:
            return std::nullopt;
        case Root::Kind::Hit:
            return root.fraction;
    }
    return std::nullopt;
}
}  // namespace

bool walk(TriangleLineHit& result, Vector3 start, Vector3 delta, std::array<Vector3, 3> vertices,
          triangle_closest::ClosestTrianglePoint closest, float radius) {
    std::uint32_t region = closest.region;
    Vector3 vertex = closest.point;
    for (int step = 0; step < 5; ++step) {
        float fraction;
        if (region <= 2) {
            bool away;
            std::optional<Fraction> hit = root_hit(roots::sphere(start, delta, vertex, radius), away);
            if (away) return false;
            std::array<Vector3, 2> adjacent;
            switch (region) {
                case 0:
                    adjacent = {sub(vertices[1], vertices[0]), sub(vertices[2], vertices[0])};
                    break;
                case 1:
                    adjacent = {sub(vertices[0], vertices[1]), sub(vertices[2], vertices[1])};
                    break;
                default:
                    adjacent = {sub(vertices[0], vertices[2]), sub(vertices[1], vertices[2])};
                    break;
            }
            std::uint32_t next_region = region;
            for (std::size_t index = 0; index < 2; ++index) {
                const Vector3 edge = adjacent[index];
                const Fraction candidate{dot(sub(vertex, start), edge), dot(delta, edge)};
                if (replaces(hit, candidate)) {
                    hit = candidate;
                    next_region = (region + (index == 0 ? 6u : 9u)) / 2u;
                }
            }
            if (!hit) return false;
            fraction = hit->value();
            if (next_region <= 2) {
                advance_fraction(result, fraction);
                result.position = madd(delta, fraction, start);
                result.normal = scale(sub(result.position, vertex), inverse_radius(radius));
                result.volume_parameter = {
                    region == 1 ? 1.0f : 0.0f,
                    region == 2 ? 1.0f : 0.0f,
                    0.0f,
                };
                return true;
            }
            region = next_region;
        } else {
            Vector3 origin, edge;
            switch (region) {
                case 3:
                    origin = vertices[0];
                    edge = sub(vertices[1], vertices[0]);
                    break;
                case 4:
                    origin = vertices[0];
                    edge = sub(vertices[2], vertices[0]);
                    break;
                default:
                    origin = vertices[1];
                    edge = sub(vertices[2], vertices[1]);
                    break;
            }
            vertex = origin;
            bool away;
            std::optional<Fraction> hit =
                root_hit(roots::cylinder(start, delta, origin, edge, radius), away);
            if (away) return false;
            const Vector3 end = add(origin, edge);
            const Fraction forward{dot(sub(end, start), edge), dot(delta, edge)};
            const Fraction backward{dot(sub(start, origin), edge), -forward.denominator};
            if (replaces(hit, forward)) {
                hit = forward;
                vertex = end;
                region /= 2;
            } else if (replaces(hit, backward)) {
                hit = backward;
                region = (region - 3) / 2;
            }
            if (!hit) return false;
            fraction = hit->value();
            if (region > 2) {
                advance_fraction(result, fraction);
                result.position = madd(delta, fraction, start);
                const float along_edge = dot(sub(result.position, origin), edge) * (1.0f / dot(edge, edge));
                const Vector3 nearest = madd(edge, along_edge, origin);
                result.normal = scale(sub(result.position, nearest), inverse_radius(radius));
                switch (region) {
                    case 3:
                        result.volume_parameter = {along_edge, 0.0f, 0.0f};
                        break;
                    case 4:
                        result.volume_parameter = {0.0f, along_edge, 0.0f};
                        break;
                    default:
                        result.volume_parameter = {1.0f - along_edge, along_edge, 0.0f};
                        break;
                }
                return true;
            }
        }
        advance_fraction(result, fraction);
        start = madd(delta, fraction, start);
        delta = scale(delta, 1.0f - fraction);
        if (fraction > 1.0f) {
            return false;
        }
    }
    // The source returns success after its fifth transition. Position/normal/
    // parameters have not been written by the walk on this path; preserve them.
    return true;
}

}  // namespace s3::physics::triangle_sweep_walk
