// Port of skate-core/src/animation/playback_tree.rs: the non-recursive items (PoseCommand,
// Evaluation) and a forward declaration of PlaybackTree. Split out (C++ only) so the mutually
// recursive child headers (phase_blend, playback_tree/blend_space, playback_tree/selection_space,
// playback_transition) can be included before PlaybackTree is complete. Include
// "s3/core/animation/playback_tree.hpp" for the full type.
#pragma once
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "s3/core/rs.hpp"

namespace s3::animation::playback_tree {

struct PlaybackTree;

/// Rust `enum PoseCommand`; each variant is a nested struct held in `value`.
struct PoseCommand {
    ///82D25B00 supplies both bounded source times and the unconsumed wrap count.
    struct Clip {
        std::string name;
        float previous_time = 0.0f;
        float time = 0.0f;
        std::uint32_t loops = 0;
    };
    ///Blend the two most recent subtree poses, preserving authored nesting.
    struct Blend {
        float weight = 0.0f;
    };
    struct WeightedBlend {
        std::vector<float> weights;
    };
    struct ChannelBlend {
        float weight = 0.0f;
        bool use_channels_from_weights = false;
    };
    struct Pose {
        std::string name;
    };
    struct Add {
        bool motion_is_a = false;
    };
    struct Mirror {
        std::uint32_t trajectory_mode = 0;
    };

    std::variant<Clip, Blend, WeightedBlend, ChannelBlend, Pose, Add, Mirror> value;

    PoseCommand(Clip v) : value(std::move(v)) {}
    PoseCommand(Blend v) : value(v) {}
    PoseCommand(WeightedBlend v) : value(std::move(v)) {}
    PoseCommand(ChannelBlend v) : value(v) {}
    PoseCommand(Pose v) : value(std::move(v)) {}
    PoseCommand(Add v) : value(v) {}
    PoseCommand(Mirror v) : value(v) {}
};
S3_DERIVE_EQ(PoseCommand::Clip, name, previous_time, time, loops)
S3_DERIVE_EQ(PoseCommand::Blend, weight)
S3_DERIVE_EQ(PoseCommand::WeightedBlend, weights)
S3_DERIVE_EQ(PoseCommand::ChannelBlend, weight, use_channels_from_weights)
S3_DERIVE_EQ(PoseCommand::Pose, name)
S3_DERIVE_EQ(PoseCommand::Add, motion_is_a)
S3_DERIVE_EQ(PoseCommand::Mirror, trajectory_mode)
S3_DERIVE_EQ(PoseCommand, value)

/// Debug-style printing (used by test failure messages).
inline std::ostream& operator<<(std::ostream& os, const PoseCommand& c) {
    if (const auto* v = std::get_if<PoseCommand::Clip>(&c.value))
        return os << "Clip { name: \"" << v->name << "\", previous_time: " << v->previous_time
                  << ", time: " << v->time << ", loops: " << v->loops << " }";
    if (const auto* v = std::get_if<PoseCommand::Blend>(&c.value)) return os << "Blend { weight: " << v->weight << " }";
    if (const auto* v = std::get_if<PoseCommand::WeightedBlend>(&c.value)) {
        os << "WeightedBlend { weights: [";
        for (std::size_t i = 0; i < v->weights.size(); ++i) os << (i ? ", " : "") << v->weights[i];
        return os << "] }";
    }
    if (const auto* v = std::get_if<PoseCommand::ChannelBlend>(&c.value))
        return os << "ChannelBlend { weight: " << v->weight
                  << ", use_channels_from_weights: " << v->use_channels_from_weights << " }";
    if (const auto* v = std::get_if<PoseCommand::Pose>(&c.value)) return os << "Pose { name: \"" << v->name << "\" }";
    if (const auto* v = std::get_if<PoseCommand::Add>(&c.value)) return os << "Add { motion_is_a: " << v->motion_is_a << " }";
    return os << "Mirror { trajectory_mode: " << std::get<PoseCommand::Mirror>(c.value).trajectory_mode << " }";
}

struct Evaluation {
    float cull_threshold = 0.0f;
    bool update_history = false;
};

}  // namespace s3::animation::playback_tree
