// Port of skate-core/src/physics/world_contact/axes.rs.
#include "s3/core/physics/world_contact/axes.hpp"

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::axes {

namespace {
using V = std::array<float, 4>;
using Out = std::array<std::array<std::uint32_t, 4>, 16>;
using reciprocal_sqrt::estimate;

V load(const std::array<std::uint32_t, 48>& gp, std::size_t offset) {
    return V{rs::from_bits(gp[offset]), rs::from_bits(gp[offset + 1]), rs::from_bits(gp[offset + 2]),
             rs::from_bits(gp[offset + 3])};
}
V sub(V a, V b) { return V{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
V cross(V a, V b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}
float dot(V a) { return arithmetic::dot({a[0], a[1], a[2]}, {a[0], a[1], a[2]}); }
float inverse_length(float squared) {
    float r = estimate(squared);
    for (int i = 0; i < 2; ++i) {
        r = rs::mul_add(r * 0.5f, rs::mul_add(-squared, r * r, 1.0f), r);
    }
    return r;
}
V normalized(V a, float squared) {
    const float reciprocal = inverse_length(squared);
    return V{a[0] * reciprocal, a[1] * reciprocal, a[2] * reciprocal, a[3] * reciprocal};
}
float length(V a) {
    const float squared = dot(a);
    if (squared == 0.0f) {
        return 0.0f;
    }
    return squared * inverse_length(squared);
}
void append(V candidate, Out& output, std::size_t& count) {
    output[count] = rs::to_bits(candidate);
    if (length(candidate) > rs::from_bits(0x34000000u)) {
        output[count] = rs::to_bits(normalized(candidate, dot(candidate)));
        count += 1;
    }
}
}  // namespace

std::size_t separating_axis_candidates(const std::array<std::uint32_t, 48>& a,
                                       const std::array<std::uint32_t, 48>& b, Out& output) {
    std::size_t count = 0;
    for (const auto* gp : {&a, &b}) {
        const std::uint32_t normals = (*gp)[35] >> 24;
        if (normals >= 1 && normals <= 3) {
            for (std::size_t index = normals; index >= 1; --index) {
                for (std::size_t k = 0; k < 4; ++k) output[count][k] = (*gp)[index * 4 + k];
                count += 1;
            }
        }
    }
    const std::size_t ae = (a[35] >> 16) & 255u;
    const std::size_t be = (b[35] >> 16) & 255u;
    if (!(ae <= 3 && be <= 3)) S3_PANIC("native GP contains at most three edges");
    for (std::size_t i = 0; i < ae; ++i) {
        for (std::size_t j = 0; j < be; ++j) {
            const V candidate = cross(load(a, 16 + 4 * i), load(b, 16 + 4 * j));
            const float length_squared = dot(candidate);
            output[count] = rs::to_bits(candidate);
            if (length_squared > rs::from_bits(0x3a83126fu)) {
                output[count] = rs::to_bits(normalized(candidate, length_squared));
                count += 1;
            }
        }
    }
    if (ae == 1 && be == 1) {
        const V edge_a = load(a, 16);
        const V edge_b = load(b, 16);
        const V perpendicular = cross(edge_a, edge_b);
        if (length(perpendicular) > rs::from_bits(0x34000000u)) {
            // Unlike the edge-pair loop, these two secondary directions are
            // unconditionally normalized once the common perpendicular passes.
            for (const V& edge : {edge_a, edge_b}) {
                const V candidate = cross(edge, perpendicular);
                output[count] = rs::to_bits(normalized(candidate, dot(candidate)));
                count += 1;
            }
        }
    }
    if (count != 0) {
        return count;
    }
    const V delta = sub(load(a, 0), load(b, 0));
    if (ae == 1 && be == 1) {
        const V edge = load(a, 16);
        append(cross(cross(delta, edge), edge), output, count);
    }
    if (ae + be == 1) {
        const V edge = ae != 0 ? load(a, 16) : load(b, 16);
        append(cross(cross(delta, edge), edge), output, count);
    }
    if (count != 0) {
        return count;
    }
    append(delta, output, count);
    return count;
}

}  // namespace s3::physics::world_contact::axes
