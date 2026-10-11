// Port of skate-core/src/physics/world_contact/reduction.rs.
#include "s3/core/physics/world_contact/reduction.hpp"

#include <algorithm>

namespace s3::physics::world_contact::reduction {

using arithmetic::cross;
using arithmetic::dot;
using arithmetic::scale;
using arithmetic::sub;
using arithmetic::vector;

bool coplanar_contacts(const ContactRecord& a, const ContactRecord& b) {
    const V normal = vector(a, 8);
    if (rs::from_bits(0x3f7fbe77u) > dot(normal, vector(b, 8))) {
        return false;
    }
    const float da = rs::abs(dot(normal, sub(vector(a, 0), vector(b, 0))));
    const float db = rs::abs(dot(normal, sub(vector(a, 4), vector(b, 4))));
    const float selected = da - db >= 0.0f ? db : da;
    return selected < rs::from_bits(0x3c23d70au);
}

std::size_t select_contact_points(const std::vector<V>& a, const std::vector<V>& b, V normal,
                                  std::array<std::uint32_t, 4>& output) {
    S3_ASSERT(!a.empty() && a.size() == b.size());
    std::size_t first = 0;
    V origin = a[0];
    float depth = dot(normal, sub(a[0], b[0]));
    for (std::size_t i = 1; i < a.size(); ++i) {
        const float candidate = dot(normal, sub(a[i], b[i]));
        if (depth > candidate) {
            depth = candidate;
            first = i;
            origin = a[i];
        }
    }
    V third_delta = sub(a[0], origin);
    V far_delta = third_delta;
    std::size_t far = 0;
    float distance = dot(far_delta, far_delta);
    for (std::size_t i = 1; i < a.size(); ++i) {
        const V delta = sub(a[i], origin);
        const float candidate = dot(delta, delta);
        if (candidate > distance) {
            distance = candidate;
            far = i;
            far_delta = delta;
        }
    }
    if (first == far) {
        output[0] = static_cast<std::uint32_t>(first);
        return 1;
    }
    std::size_t third = 0;
    const V initial_area = cross(third_delta, far_delta);
    float area = dot(initial_area, initial_area);
    for (std::size_t i = 1; i < a.size(); ++i) {
        const V delta = sub(a[i], origin);
        const V area_vector = cross(delta, far_delta);
        const float candidate = dot(area_vector, area_vector);
        if (candidate > area) {
            area = candidate;
            third = i;
            third_delta = delta;
        }
    }
    output[0] = static_cast<std::uint32_t>(first);
    output[1] = static_cast<std::uint32_t>(far);
    output[2] = static_cast<std::uint32_t>(third);
    if (third == first || third == far) {
        return 2;
    }
    const V side = cross(normal, far_delta);
    const float sign = dot(side, third_delta) >= 0.0f ? 1.0f : -1.0f;
    const V far_side = scale(side, sign);
    const V third_side = scale(cross(normal, third_delta), sign);
    const V edge = sub(third_delta, far_delta);
    std::size_t fourth = 9999;
    float best = -1.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const V delta = sub(a[i], origin);
        V selected;
        if (dot(delta, far_side) > 0.0f) {
            if (dot(delta, third_side) > 0.0f) {
                selected = cross(delta, third_delta);
            } else {
                selected = cross(sub(delta, far_delta), edge);
            }
        } else {
            selected = cross(delta, far_delta);
        }
        const float candidate = dot(selected, selected);
        if (candidate > best) {
            best = candidate;
            fourth = i;
        }
    }
    output[3] = static_cast<std::uint32_t>(fourth);
    return 3 + std::size_t(fourth != first && fourth != far && fourth != third && fourth != 9999);
}

void reduce(std::array<ContactRecord, 50>& records, std::uint32_t& count) {
    const std::size_t original = count;
    S3_ASSERT(original <= 50);
    std::array<bool, 50> assigned{};
    std::vector<std::size_t> retained;
    retained.reserve(original);
    for (std::size_t first = 0; first < original; ++first) {
        if (assigned[first]) {
            continue;
        }
        assigned[first] = true;
        std::vector<std::size_t> group{first};
        for (std::size_t next = first + 1; next < original; ++next) {
            if (!assigned[next] && records[first][3] == records[next][3] &&
                records[first][7] == records[next][7] && coplanar_contacts(records[first], records[next])) {
                assigned[next] = true;
                group.push_back(next);
            }
        }
        if (group.size() <= 4) {
            retained.insert(retained.end(), group.begin(), group.end());
        } else {
            std::vector<V> a, b;
            for (std::size_t i : group) a.push_back(vector(records[i], 0));
            for (std::size_t i : group) b.push_back(vector(records[i], 4));
            std::array<std::uint32_t, 4> selected{0, 1, 2, 3};
            const std::size_t length = select_contact_points(a, b, vector(records[first], 8), selected);
            for (std::size_t k = 0; k < length; ++k) retained.push_back(group[selected[k]]);
        }
    }
    if (retained.size() < original) {
        std::sort(retained.begin(), retained.end());
        count = static_cast<std::uint32_t>(retained.size());
        for (std::size_t destination = 0; destination < retained.size(); ++destination) {
            const std::size_t source = retained[destination];
            if (destination != source) {
                // Native 8277A300 loads/stores float fields individually. Its
                // f32→f64→f32 copy quiets sNaNs; IDs/tag remain integer stores.
                for (std::size_t k = 0; k < 64; ++k) {
                    const std::uint32_t bits = records[source][k];
                    records[destination][k] =
                        ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0)
                            ? (bits | 0x00400000u)
                            : bits;
                }
                for (std::size_t i : {3u, 7u, 23u}) {
                    records[destination][i] = records[source][i];
                }
            }
        }
    }
}

}  // namespace s3::physics::world_contact::reduction
