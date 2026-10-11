// TEMP DEP: minimal port of the skate-net crate items used by skate-game/src/physics/network.rs
// (skate-net/src/lib.rs: Pose, Body, Bone, hash; skate-net/src/packed.rs: BodyState, PoseState).
// The multiplayer transport itself is not ported; namespace `s3::net` mirrors `skate_net`.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "s3/core/rs.hpp"

namespace s3::net {

struct Pose {
    std::array<float, 3> p{};
    std::array<float, 4> q{};

    bool valid() const {
        for (float v : p)
            if (!(rs::is_finite(v) && rs::abs(v) < 100000.0f)) return false;
        for (float v : q)
            if (!rs::is_finite(v)) return false;
        float s = -0.0f;
        for (float v : q) s += v * v;
        return s >= 0.98f && s < 1.02f;
    }
};
S3_DERIVE_EQ(Pose, p, q)

struct Body {
    Pose pose{};
    std::array<float, 3> velocity{};
    std::array<float, 3> angular{};
};

struct Bone {
    std::uint16_t index = 0;
    Pose pose{};
};

/// FNV-1a 64.
inline std::uint64_t hash(const std::uint8_t* bytes, std::size_t len) {
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (std::size_t i = 0; i < len; ++i) h = (h ^ std::uint64_t(bytes[i])) * 0x100000001b3ull;
    return h;
}

namespace packed {

struct BodyState {
    Pose root{};
    std::uint64_t enabled = 0;
    std::vector<Body> bodies;
};

struct PoseState {
    Pose root{};
    std::vector<Bone> bones;
};

}  // namespace packed

}  // namespace s3::net
