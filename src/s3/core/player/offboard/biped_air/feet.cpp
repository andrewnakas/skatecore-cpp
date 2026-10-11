// Port of skate-core/src/player/offboard/biped_air/feet.rs (whole file).
#include "s3/core/player/offboard/biped_air/feet.hpp"

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/player/offboard/air_launch/math.hpp"
#include "s3/core/player/wipeout_state/math.hpp"

namespace s3::player::offboard::biped_air::recovered::feet {

using air_launch::math::dot;
using air_launch::math::length;
using air_launch::math::limit_angle;
using air_launch::math::madd;
using air_launch::math::scale;
using air_launch::math::sub;
using physics::skeleton_animation_record::transform_point;
using wipeout_state::math::reciprocal;
inline V unit(V a, V fallback) { return wipeout_state::math::normalize_or(a, fallback); }

namespace {
float foot_height(const State& s, const Input& i, float ground, float desired) {
    const float base = ground + 0.02f;
    const float proposed = (desired - s.scalars_288_to_296[0]) + ground;
    const float limited = rs::max(rs::min(proposed, base + 0.08f), base);
    const float output = rs::max(desired, limited);
    const float ceiling = rs::mul_add(i.velocity[1], rs::from_bits(0x3c888889u), i.position[1]) - 0.46f;
    if (output - desired > 0.4f || output > ceiling) {
        return desired;
    }
    return output;
}

V clamp_normal(V normal) {
    if (normal[1] >= 0.9f) {
        return normal;
    }
    normal[1] = 0.0f;
    // 82D78260 does NOT safe-normalize this flattened normal.
    // Verified image822F8BD8 =3E428F5F; retain the unguarded reciprocal.
    normal = scale(normal, reciprocal(length(normal)) * rs::sqrt(rs::from_bits(0x3e428f5fu)));
    normal[1] = 0.9f;
    return normal;
}

float height_correction(State& s, const Input& i) {
    if (!s.hands[0].flag_84 && !s.hands[1].flag_84) {
        return 0.0f;
    }
    float highest = rs::from_bits(0x501502f9u);
    for (auto& foot : s.hands) {
        foot.scalars_96_100[0] = 0.0f;
        if (!foot.flag_84) {
            continue;
        }
        if (foot.flags_104_to_107[0]) {
            const V normal0 = foot.vectors_0_to_64[4];
            const V side = i.effective_root[0];
            const V normal = unit(sub(normal0, scale(side, dot(side, normal0))), V{});
            foot.scalars_96_100[0] = dot(normal, i.effective_root[2]) * 0.2f;
        }
        highest = rs::min(highest, foot.vectors_0_to_64[3][1] + foot.scalars_96_100[0]);
    }
    return s.scalars_288_to_296[0] - highest;
}
}  // namespace

std::array<Target, 2> update(State& s, const Input& i) {
    for (std::size_t index = 0; index < 2; ++index) {
        auto& foot = s.hands[index];
        const Line& line = i.lines[index];
        foot.vectors_0_to_64[1] = i.world_foot_pairs[index][0];
        foot.vectors_0_to_64[3] = line.position;
        foot.vectors_0_to_64[4] = line.normal;
        foot.word_80 = line.surface;
        foot.flag_84 = line.valid;
        const std::size_t side = index ^ static_cast<std::size_t>((i.flags_2476 & 4u) != 0);
        foot.flags_104_to_107[2] = (i.flags_2484 & (1u << (6 + side))) != 0;
        foot.flags_104_to_107[3] = (i.flags_2484 & (1u << (2 + side))) != 0;
    }
    if (s.words_308_to_316[0] == 0) {
        s.vectors_224_to_272[2] = i.effective_root[3];
        s.flags_304_to_307[3] = true;
    }
    // 82D783A8: evaluate the old support mode before82D77CE0 replaces it.
    for (std::size_t index = 0; index < 2; ++index) {
        auto& foot = s.hands[index];
        const bool low = s.word_300 != 3 && i.local_foot_pairs[index][0][1] < 0.09f &&
                         i.local_foot_pairs[index][1][1] < 0.09f;
        foot.flags_104_to_107[0] = low;
        bool supported;
        if (foot.flag_84) {
            const V normal = clamp_normal(foot.vectors_0_to_64[4]);
            foot.vectors_0_to_64[2] = normal;
            const float d0 = dot(normal, sub(i.world_foot_pairs[index][0], foot.vectors_0_to_64[3]));
            const float d1 = dot(normal, sub(i.world_foot_pairs[index][1], foot.vectors_0_to_64[3]));
            const float distance = rs::max(d0, d1);
            supported = distance < 0.09f || (low && distance < 0.2f);
        } else {
            foot.vectors_0_to_64[2] = V{0.0f, 1.0f, 0.0f, 0.0f};
            supported = low;
        }
        foot.flags_104_to_107[1] = supported;
    }
    const bool mirrored = (i.flags_2476 & 4u) != 0;
    const bool forced_left = (i.flags_2480 & (mirrored ? (1u << 5) : (1u << 6))) != 0;
    const bool forced_right = (i.flags_2480 & (mirrored ? (1u << 6) : (1u << 5))) != 0;
    if (forced_left) {
        s.hands[0].flags_104_to_107[1] = true;
        s.hands[1].flags_104_to_107[1] = false;
    } else if (forced_right) {
        s.hands[0].flags_104_to_107[1] = false;
        s.hands[1].flags_104_to_107[1] = true;
    }
    s.scalars_288_to_296[1] += rs::from_bits(0x3c888889u);
    std::uint32_t support;
    if (i.state == 501) {
        support = 3;
    } else if (s.flags_304_to_307[0]) {
        const bool a = s.hands[0].flags_104_to_107[1], b = s.hands[1].flags_104_to_107[1];
        support = (a && !b) ? 0u : (!a && b) ? 1u : 2u;
    } else {
        support = 2;
    }
    if (support != s.word_300) {
        s.word_300 = support;
        s.scalars_288_to_296[1] = 0.0f;
        s.vectors_224_to_272[3] = support < 2 ? i.world_foot_pairs[support][0] : V{};
    }
    for (std::size_t index = 0; index < 2; ++index) {
        const std::uint32_t support_index = static_cast<std::uint32_t>(index);
        const auto foot = s.hands[index];
        V desired = foot.vectors_0_to_64[1];
        if (s.word_300 == support_index) {
            V flat = sub(s.vectors_224_to_272[3], desired);
            flat[1] = 0.0f;
            if (dot(flat, flat) < rs::from_bits(0x3bd1b717u)) {
                desired = s.vectors_224_to_272[3];
            } else {
                const V old = s.vectors_224_to_272[3];
                const float distance = length(flat);
                desired = madd(flat, 0.08f / distance, desired);
                desired[1] = old[1];
                s.vectors_224_to_272[3] = desired;
            }
            s.hands[index].vectors_0_to_64[0] = desired;
        } else {
            if (foot.flag_84) {
                desired[1] = foot_height(s, i, foot.vectors_0_to_64[3][1],
                                         desired[1] - rs::max(foot.scalars_96_100[0], 0.0f));
            }
            const float time = rs::max(foot.scalars_96_100[1] - rs::from_bits(0x3c888889u), 0.0f);
            s.hands[index].scalars_96_100[1] = time;
            const float weight = 1.0f - time * 10.0f;
            s.hands[index].vectors_0_to_64[0] = madd(desired, weight, scale(foot.vectors_0_to_64[0], 1.0f - weight));
        }
    }
    std::array<Target, 2> targets{};
    for (std::size_t index = 0; index < 2; ++index) {
        auto& foot = s.hands[index];
        const bool world = s.word_300 == static_cast<std::uint32_t>(index);
        if (world && (i.flags_2484 & 0x80000000u) == 0) {
            foot.scalars_96_100[1] = 0.1f;
        }
        Target& t = targets[index];
        t.position = world ? foot.vectors_0_to_64[0] : transform_point(i.inverse_root, foot.vectors_0_to_64[0]);
        t.world = world;
        t.blend = world ? 1.0f : 0.3f;
        if (foot.flags_104_to_107[1] && foot.flag_84) t.normal = foot.vectors_0_to_64[2];
    }
    s.scalars_288_to_296[0] = i.root[3][1];
    s.scalars_288_to_296[2] = height_correction(s, i);
    return targets;
}

void set_normal(animation::foot_ik::external::ExternalTarget& target, V normal) {
    if (target.normal_blend > 0.0f) {
        target.normal = limit_angle(normal, target.normal, 0.1f);
    }
    target.normal_set = true;
    target.normal_blend = rs::min(target.normal_blend + 0.2f, 1.0f);
}

}  // namespace s3::player::offboard::biped_air::recovered::feet
