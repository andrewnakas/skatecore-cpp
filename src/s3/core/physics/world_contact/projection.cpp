// Port of skate-core/src/physics/world_contact/projection.rs.
#include "s3/core/physics/world_contact/projection.hpp"

#include "s3/core/physics/world_contact/axes.hpp"

namespace s3::physics::world_contact::projection {

using arithmetic::dot;
using arithmetic::scale;
using arithmetic::vector;
using axes::separating_axis_candidates;

namespace {
void project(const std::array<std::uint32_t, 48>& gp, PrimitiveKind kind, std::array<std::uint32_t, 4> direction_bits,
             std::array<std::uint32_t, 12>& output, bool batch) {
    const auto direction = vector(direction_bits, 0);
    const float center = dot(vector(gp, 0), direction);
    float min, max;
    switch (kind) {
        case PrimitiveKind::Sphere:
            min = center;
            max = center;
            break;
        case PrimitiveKind::Capsule: {
            const float radius = rs::abs(dot(vector(gp, 16), direction)) * rs::from_bits(gp[28]);
            min = center - radius;
            max = center + radius;
            break;
        }
        case PrimitiveKind::Triangle: {
            const float second = dot(vector(gp, 8), direction);
            const float third = dot(vector(gp, 12), direction);
            // VMX min/max select the second operand on unordered or equal.
            const float lo = center < second ? center : second;
            const float hi = center > second ? center : second;
            min = lo < third ? lo : third;
            max = hi > third ? hi : third;
            break;
        }
        case PrimitiveKind::Box:
        default: {
            float extent[3];
            for (std::size_t i = 0; i < 3; ++i) {
                const auto axis = vector(gp, 4 + i * 4);
                const float half = rs::from_bits(gp[28 + i]);
                extent[i] = batch ? rs::abs(dot(scale(axis, half), direction)) : rs::abs(dot(axis, direction) * half);
            }
            const float radius = (extent[0] + extent[1]) + extent[2];
            min = center - radius;
            max = center + radius;
            break;
        }
    }
    for (std::size_t i = 0; i < 4; ++i) output[i] = rs::to_bits(min);
    for (std::size_t i = 4; i < 8; ++i) output[i] = rs::to_bits(max);
}
}  // namespace

void project_direction(const std::array<std::uint32_t, 48>& gp, PrimitiveKind kind,
                       std::array<std::uint32_t, 4> direction, std::array<std::uint32_t, 12>& output) {
    project(gp, kind, direction, output, false);
}

void project_directions(const std::array<std::uint32_t, 48>& gp, PrimitiveKind kind,
                        const std::array<std::uint32_t, 4>* directions, std::size_t count,
                        std::array<std::uint32_t, 12>* output, std::size_t output_len) {
    S3_ASSERT(output_len >= count);
    for (std::size_t i = 0; i < count; ++i) {
        project(gp, kind, directions[i], output[i], true);
    }
}

std::pair<std::array<std::uint32_t, 4>, std::array<std::uint32_t, 4>> best_separating_direction(
    const std::array<std::uint32_t, 48>& a, PrimitiveKind a_kind, const std::array<std::uint32_t, 48>& b,
    PrimitiveKind b_kind) {
    std::array<std::array<std::uint32_t, 4>, 16> directions{};
    std::size_t count = separating_axis_candidates(a, b, directions);
    if (count == 0) {
        directions[0] = {0, 0, rs::to_bits(1.0f), 0};
        count = 1;
    }
    std::array<std::array<std::uint32_t, 12>, 16> a_intervals{};
    std::array<std::array<std::uint32_t, 12>, 16> b_intervals{};
    project_directions(a, a_kind, directions.data(), count, a_intervals.data(), a_intervals.size());
    project_directions(b, b_kind, directions.data(), count, b_intervals.data(), b_intervals.size());
    std::size_t selected = 0;
    float best = 0.0f;
    bool flip = false;
    for (std::size_t index = 0; index < count; ++index) {
        const float forward = rs::from_bits(a_intervals[index][0]) - rs::from_bits(b_intervals[index][4]);
        const float reverse = rs::from_bits(b_intervals[index][0]) - rs::from_bits(a_intervals[index][4]);
        // fcmpu followed by ble (!gt): unordered selects reverse without flip.
        const bool reversed = forward > reverse;
        const float separation = reversed ? forward : reverse;
        if (index == 0 || separation > best) {
            best = separation;
            selected = index;
            flip = reversed;
        }
    }
    std::array<std::uint32_t, 4> normal = directions[selected];
    if (flip) {
        for (auto& v : normal) v ^= 0x80000000u;
    }
    const std::uint32_t b_bits = rs::to_bits(best);
    return {{b_bits, b_bits, b_bits, b_bits}, normal};
}

}  // namespace s3::physics::world_contact::projection
