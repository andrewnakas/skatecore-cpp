// Port of skate-game/src/physics/handplant/contact.rs
//! PositionSelector82D63D40, velocity window82D64420 and edge clipping82D651E0.
#include "s3/game/physics/handplant/contact.hpp"

#include <utility>
#include <vector>

#include "s3/core/air/trajectory.hpp"
#include "s3/core/animation/foot_ik/transforms.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/trigonometry.hpp"
#include "s3/game/physics/footplant/math.hpp"
#include "s3/game/physics/handplant.hpp"

namespace s3::game::physics::handplant::contact {

using namespace ::s3::game::physics::footplant::math;
using ::s3::air::trajectory::Trajectory;
using ::s3::animation::foot_ik::transforms::inverse_rigid;
using ::s3::physics::skeleton_animation_record::IDENTITY;
using handplant::GRAVITY;
using handplant::UP;

std::optional<Candidate> select(const Settings& settings, V com, V velocity, V normal, std::int32_t hint,
                                ::s3::physics::Slice<const Primitive> edges) {
    //82D61D88: ascending, above minimum speed, slope over45deg and |Ny|<=.95.
    if (!(length(velocity) > settings.minimum_speed && velocity[1] > 0.0f && rs::abs(normal[1]) <= 0.95f &&
          ::s3::trigonometry::acos(rs::clamp(normal[1], -1.0f, 1.0f)) * rs::from_bits(0x42652ee1u) >
              settings.minimum_slope)) {
        return std::nullopt;
    }
    auto frame = IDENTITY;
    if (rs::abs(normal[1]) <= 0.99f) {
        frame[0] = normalize(cross(UP, normal));
        frame[1] = UP;
        frame[2] = cross(frame[0], UP);
    }
    frame[3] = com;
    const auto inverse = inverse_rigid(frame);
    velocity = rotate(inverse, velocity);
    velocity = {velocity[0], rs::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1]), 0.0f, 0.0f};
    if (hint != 0 && hint != (velocity[0] > 0.0f ? 1 : 2)) {
        velocity[0] = (hint == 1 ? 1.0f : -1.0f) * rs::from_bits(0x37800000u);
    }
    std::array<float, 7> g{};
    for (std::size_t i = 0; i < 7; ++i) g[i] = settings.window[i].evaluate(velocity[1]);
    const float lo_x = rs::max(g[1], rs::abs(velocity[0]) - g[5]);
    const float hi_x = rs::min(g[6], rs::abs(velocity[0]) + g[4]);
    if (hi_x <= lo_x) {
        return std::nullopt;
    }
    const float lo_y = rs::max(g[0], velocity[1] - g[3]);
    const float hi_y = velocity[1] + g[2];
    const float sign = velocity[0] > 0.0f ? 1.0f : -1.0f;
    auto apex = [&](float x, float y) {
        const Trajectory trajectory{point(inverse, com), {x * sign, y, 0.0f, 0.0f}, GRAVITY, -1.0f};
        V p = trajectory.position_at(apex_time(trajectory));
        p[1] -= settings.window_drop;
        return p;
    };
    //Native order:r5 lowX/lowY,r6 highX/lowY,r7 lowX/highY,r8 highX/highY.
    const V a = apex(lo_x, lo_y);
    const V b = apex(hi_x, lo_y);
    const V c = apex(lo_x, hi_y);
    const V d = apex(hi_x, hi_y);
    const std::array<V, 4> polygon = d[0] > c[0] ? std::array<V, 4>{c, d, b, a} : std::array<V, 4>{d, c, a, b};
    //82C1EAD8 preserves world order and caps the broadphase result at40.
    std::vector<std::pair<Primitive, std::array<V, 2>>> candidates;
    std::size_t taken = 0;
    for (const auto& edge : edges) {
        bool overlaps = true;
        for (std::size_t i = 0; i < 3; ++i) {
            const float extent = std::array<float, 3>{2.0f, 4.0f, 2.0f}[i];
            overlaps &= rs::min(edge.start[i], edge.end[i]) <= com[i] + extent &&
                        rs::max(edge.start[i], edge.end[i]) >= com[i] - extent;
            if (!overlaps) break;
        }
        if (!overlaps) continue;
        if (taken == 40) break;
        ++taken;
        const V start = point(inverse, edge.start);
        const V end = point(inverse, edge.end);
        if (rs::abs(normalize(sub(end, start))[0]) <= 0.71f) {
            continue;
        }  //82D64218
        std::array<V, 2> segment{start, end};
        //82D657D0/658A0 retains -depth<localZ<0.
        if (!clip_depth(segment, -settings.depth, true) || !clip_depth(segment, 0.0f, false)) {
            continue;
        }
        bool valid = true;
        for (std::size_t i = 0; i < 4; ++i) {
            const V delta = sub(polygon[(i + 1) % 4], polygon[i]);
            const V n = normalize({delta[1], -delta[0], 0.0f, 0.0f});
            const float d0 = dot(sub(segment[0], polygon[i]), n);
            const float d1 = dot(sub(segment[1], polygon[i]), n);
            if (!clip(segment, d0, d1)) {
                valid = false;
                break;
            }
        }
        if (valid) {
            candidates.emplace_back(edge, segment);
        }
    }
    std::optional<std::size_t> best;
    float height = -rs::f32::MAX;
    float depth = -rs::f32::MAX;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const auto& segment = candidates[i].second;
        const V p = segment[0][1] > segment[1][1] ? segment[0] : segment[1];
        if (p[1] > height + 0.1f || (p[1] > height - 0.1f && p[1] <= height + 0.1f && p[2] > depth)) {
            best = i;
            height = p[1];
            depth = p[2];
        }
    }
    if (!best) return std::nullopt;
    const std::size_t best_i = *best;
    std::vector<bool> connected(candidates.size(), false);
    connected[best_i] = true;
    //82D65BD8: extend both ends across neighbours within .05m and30degrees.
    for (bool reverse : {false, true}) {
        auto ends = candidates[best_i].second;
        if (reverse) {
            std::swap(ends[0], ends[1]);
        }
        while (true) {
            std::optional<std::pair<std::size_t, std::array<V, 2>>> next;
            for (std::size_t i = 0; i < candidates.size() && !next; ++i) {
                if (connected[i]) continue;
                const auto& s = candidates[i].second;
                for (std::size_t e = 0; e < 2; ++e) {
                    if (dot(sub(ends[1], s[e]), sub(ends[1], s[e])) < rs::from_bits(0x3b23d70au) &&
                        rs::abs(::s3::trigonometry::acos(rs::clamp(
                            dot(normalize(sub(ends[1], ends[0])), normalize(sub(s[1 - e], s[e]))), -1.0f, 1.0f))) <
                            rs::from_bits(0x3f060a92u)) {
                        next = std::make_pair(i, std::array<V, 2>{s[e], s[1 - e]});
                        break;
                    }
                }
            }
            if (!next) break;
            connected[next->first] = true;
            ends = next->second;
        }
    }
    const V bottom = scale(add(polygon[2], polygon[3]), 0.5f);
    const V top = scale(add(polygon[0], polygon[1]), 0.5f);
    const V delta = sub(top, bottom);
    V target = madd(delta, clamp01((height - bottom[1]) * reciprocal(delta[1])), bottom);
    target = {target[0], target[1], 0.0f, 0.0f};
    V nearest{};
    float distance = rs::f32::MAX;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        if (!connected[i]) continue;
        const auto& segment = candidates[i].second;
        const V seg_delta = sub(segment[1], segment[0]);
        const float square = dot(seg_delta, seg_delta);
        V p{};
        if (square > rs::from_bits(0x37800000u)) {
            p = madd(seg_delta, clamp01(dot(sub(target, segment[0]), seg_delta) * reciprocal(square)), segment[0]);
        } else {
            p = segment[0];
        }
        const float sq = dot(sub(p, target), sub(p, target));
        if (sq < distance) {
            distance = sq;
            nearest = p;
        }
    }
    return Candidate{point(frame, nearest), candidates[best_i].first, velocity[0] > 0.0f ? 1 : 2};
}

//82D658A0 excludes coplanar segments and avoids dividing near-parallel Z.
bool clip_depth(std::array<V, 2>& segment, float value, bool above) {
    const float sign = above ? 1.0f : -1.0f;
    const float a = (segment[0][2] - value) * sign;
    const float b = (segment[1][2] - value) * sign;
    if (a <= 0.0f && b <= 0.0f) {
        return false;
    }
    const float delta = segment[1][2] - segment[0][2];
    if (rs::abs(delta) >= rs::from_bits(0x37800000u)) {
        const float t = (value - segment[0][2]) * reciprocal(delta);
        if (t > 0.0f && t < 1.0f) {
            const V p = madd(sub(segment[1], segment[0]), t, segment[0]);
            segment[b <= 0.0f ? 1 : 0] = p;
        }
    }
    return true;
}

bool clip(std::array<V, 2>& segment, float a, float b) {
    if (a < 0.0f && b < 0.0f) {
        return false;
    }
    if ((a < 0.0f) != (b < 0.0f)) {
        const V p = madd(sub(segment[1], segment[0]), a * reciprocal(a - b), segment[0]);
        segment[a >= 0.0f ? 1 : 0] = p;
    }
    return true;
}

}  // namespace s3::game::physics::handplant::contact
