// Port of skate-core/src/physics/triangle_closest.rs.
#include "s3/core/physics/triangle_closest.hpp"

#include "s3/core/physics/triangle_query.hpp"

namespace s3::physics::triangle_closest {

using triangle_query::dot;
using triangle_query::madd;
using triangle_query::sub;

ClosestTrianglePoint closest(Vector3 point, std::array<Vector3, 3> vertices) {
    const Vector3 origin = vertices[0], b = vertices[1], c = vertices[2];
    const Vector3 edge_b = sub(b, origin);
    const Vector3 edge_c = sub(c, origin);
    const Vector3 toward_origin = sub(origin, point);
    const float bb = dot(edge_b, edge_b);
    const float cc = dot(edge_c, edge_c);
    const float bc = dot(edge_b, edge_c);
    const float bp = dot(edge_b, toward_origin);
    const float cp = dot(edge_c, toward_origin);
    const float determinant = rs::mul_add(cc, bb, -(bc * bc));
    const float u_numerator = rs::mul_add(cp, bc, -(bp * cc));
    const float v_numerator = rs::mul_add(bp, bc, -(cp * bb));
    const float opposite_length_squared = rs::mul_add(-bc, 2.0f, bb) + cc;
    int feature;
    if (determinant < rs::from_bits(0x00200000u)) {
        if (bb > cc) {
            feature = bb > opposite_length_squared ? 0 : 2;
        } else if (cc > opposite_length_squared) {
            feature = 1;
        } else {
            feature = 2;
        }
    } else if (u_numerator + v_numerator > determinant) {
        if (u_numerator < 0.0f) {
            feature = cp + cc < bp + bc ? 1 : 2;
        } else if (v_numerator < 0.0f) {
            feature = bp + bb < cp + bc ? 0 : 2;
        } else {
            feature = 2;
        }
    } else if (u_numerator < 0.0f) {
        feature = -cp > 0.0f ? 1 : 0;
    } else if (v_numerator < 0.0f) {
        feature = -bp > 0.0f ? 0 : 1;
    } else {
        feature = 3;
    }

    float u, v;
    std::uint32_t region;
    switch (feature) {
        case 0:
            if (!(bp < 0.0f)) {
                u = 0.0f; v = 0.0f; region = 0;
            } else if (!(-bp < bb)) {
                u = 1.0f; v = 0.0f; region = 1;
            } else {
                u = -(bp / bb); v = 0.0f; region = 3;
            }
            break;
        case 1:
            if (!(cp < 0.0f)) {
                u = 0.0f; v = 0.0f; region = 0;
            } else if (!(-cp < cc)) {
                u = 0.0f; v = 1.0f; region = 2;
            } else {
                u = 0.0f; v = -(cp / cc); region = 4;
            }
            break;
        case 2: {
            const float numerator = ((cp + cc) - bc) - bp;
            if (!(numerator > 0.0f)) {
                u = 0.0f; v = 1.0f; region = 2;
            } else if (!(numerator < opposite_length_squared)) {
                u = 1.0f; v = 0.0f; region = 1;
            } else {
                u = numerator / opposite_length_squared;
                v = 1.0f - u;
                region = 5;
            }
            break;
        }
        default: {
            const float inverse = 1.0f / determinant;
            u = inverse * u_numerator;
            v = inverse * v_numerator;
            region = 6;
            break;
        }
    }
    return ClosestTrianglePoint{madd(edge_c, v, madd(edge_b, u, origin)), region, u, v};
}

}  // namespace s3::physics::triangle_closest
