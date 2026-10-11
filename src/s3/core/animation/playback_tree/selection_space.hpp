// Port of skate-core/src/animation/playback_tree/selection_space.rs.
//! Type11's authored metric and first-update selection: 82D26CC0/82D26FF8.
//! Native construction clears transition/reselect bits. No exposed operation
//! on this tree enables them; ordinary playback therefore latches one child.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/animation/boxed.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/playback_tree_fwd.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::playback_tree::selection_space {

using output::attributes::AttributeName;
using playback_parameters::SettableAttribute;

struct Parameter {
    AttributeName name;
    std::uint32_t mode = 0;
    float weight = 0.0f;
    float minimum = 0.0f;
    float maximum = 0.0f;
};

struct Candidate {
    std::string name;
    std::vector<float> values;
    /// Rust `tree: PlaybackTree` (by value); boxed so Candidate is declarable while PlaybackTree
    /// is incomplete. Deep-copying; access with `*candidate.tree` / `candidate.tree->`.
    Box<PlaybackTree> tree;
};

struct SelectionSpace {
    std::vector<Parameter> parameters;
    std::vector<Candidate> candidates;
    std::optional<std::size_t> selected;
    std::vector<std::optional<float>> values;
    float speed = 0.0f;
    float requested_time = 0.0f;

    static rs::Result<SelectionSpace, std::string> new_(std::vector<Parameter> parameters,
                                                        std::vector<Candidate> candidates);
    /// `Option<&PlaybackTree>`: nullptr before selection.
    const PlaybackTree* current() const;
    PlaybackTree* current_mut();
    void set_time(float time);
    void set_speed(float speed);
    rs::Result<rs::Unit, std::string> select(Slice<const SettableAttribute> attributes);
    rs::Result<bool, std::string> set_attributes(Slice<const SettableAttribute> attributes);
};

/// Original 82D26FF8 uses two accumulators and FMA for pairs, then a separate
/// multiply/multiply/add for an odd tail. Do not reorder into a generic sum.
float distance(Slice<const Parameter> parameters, Slice<const float> values, Slice<const float> candidate);
float delta(const Parameter& parameter, float value, float candidate);

}  // namespace s3::animation::playback_tree::selection_space

// Completes PlaybackTree for includers of this header (no-op when already in progress).
#include "s3/core/animation/playback_tree.hpp"
