// Port of skate-game/src/graph_host/motion_animation/tree_builder.rs.
//! Stock authored tree construction; runtime selection remains in the tree.
#pragma once
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/playback_tree.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/animation_metadata.hpp"

namespace s3::game::graph_host::motion_animation::tree_builder {

/// `pub(super) fn build`. `parents` holds the uppercase names of the trees under construction.
rs::Result<s3::animation::playback_tree::PlaybackTree, std::string> build(
    const s3::data::AnimationMetadata& metadata, std::string_view name,
    s3::animation::Slice<const std::pair<s3::animation::output::attributes::AttributeName,
                                         s3::animation::output::attributes::AttributeName>>
        construction,
    std::vector<std::string>& parents);

}  // namespace s3::game::graph_host::motion_animation::tree_builder
