// Port of skate-core/src/player/offboard/contact_toolkit/probes.rs (whole file).
// Original descriptors82D856C8 and query geometry82D811C8.
#pragma once
#include <optional>
#include <vector>

#include "s3/core/player/_deps/air_trajectory.hpp"
#include "s3/core/player/offboard/contact_toolkit/base.hpp"

namespace s3::player::offboard::contact_toolkit {

struct Input {
    Vector position{};         // toolkit0
    Vector forward{};          // 16
    Vector up{};               // 32
    Vector right{};            // 48
    Vector velocity{};         // 64
    Vector animation_up{};     // 80
    Vector animation_right{};  // 96

    static Input from_vectors(const std::array<Vector, 7>& v) { return Input{v[0], v[1], v[2], v[3], v[4], v[5], v[6]}; }
};
S3_DERIVE_EQ(Input, position, forward, up, right, velocity, animation_up, animation_right)

struct LineProbe {
    Vector start{};
    Vector end{};
    float radius = 0.0f;
};
S3_DERIVE_EQ(LineProbe, start, end, radius)

struct Descriptor {
    LineProbe line{};
    std::size_t forward_index = 0;
    std::optional<std::size_t> reverse_index;
};
S3_DERIVE_EQ(Descriptor, line, forward_index, reverse_index)

struct Batch {
    Input input{};
    std::int32_t matching_group = 0;
    /// Original request mesh184 rejection mask. Not a surface-material mask.
    std::uint32_t mesh_reject_mask = 0;
    std::array<air::trajectory::QueryRequest, 3> trajectories{};
    std::vector<LineProbe> lines;
    std::vector<Descriptor> secondary;
    std::vector<Descriptor> primary;
};

struct ProbeLayout {
    std::array<LineProbe, 3> trajectories{};
    std::vector<Descriptor> secondary;
    std::vector<Descriptor> primary;

    static ProbeLayout stock();
    Batch prepare(const Input& input, std::int32_t matching_group) const;
};
S3_DERIVE_EQ(ProbeLayout, trajectories, secondary, primary)

}  // namespace s3::player::offboard::contact_toolkit
