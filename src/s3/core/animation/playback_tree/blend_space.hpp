// Port of skate-core/src/animation/playback_tree/blend_space.rs.
//! Andale BlendSpace82D22DA0..82D24B90. Authored simplex planes, original
//! scalar selection82D23A50 and phase-coupled child clocks; no fitted metric.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "s3/core/animation/clip_clock.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/playback_tree_fwd.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::playback_tree::blend_space {

using clip_clock::AdvanceResult;
using output::attributes::AnimationAttribute;
using output::attributes::AttributeName;
using playback_parameters::SettableAttribute;

struct Simplex {
    std::vector<std::size_t> children;
    std::vector<std::vector<float>> vertices;
    std::vector<std::vector<float>> normals;
    std::vector<float> scales;

    ///82D23898: signed distances to the opposite planes, scaled by inverse height.
    std::vector<float> coordinates(Slice<const float> point) const;
    ///82D23530/82D23080: successive coordinate-plane slices, retaining edge
    ///enumeration order. On an empty slice choose its nearest remaining vertex.
    /// (Private in Rust; public for the ported tests.)
    std::vector<float> project(Slice<const float> point) const;
};

struct BlendSpace {
    std::vector<AttributeName> parameters;
    std::vector<PlaybackTree> children;
    std::vector<Simplex> simplexes;
    std::vector<float> values;
    std::size_t current = 0;
    std::vector<float> weights;
    float time = 0.0f;

    static rs::Result<BlendSpace, std::string> new_(std::vector<AttributeName> parameters,
                                                    std::vector<PlaybackTree> children,
                                                    std::vector<Simplex> simplexes);
    /// `(child index, weight)` pairs of the active simplex (Rust iterator collected).
    std::vector<std::pair<std::size_t, float>> active_weights() const;
    float length() const;
    void set_time(float time);
    void set_speed(float speed);
    void advance(float dt, float phase, AdvanceResult& property);
    rs::Result<bool, std::string> set_attributes(Slice<const SettableAttribute> attributes);
    rs::Result<std::vector<AnimationAttribute>, std::string> attributes(std::uint32_t mask) const;
    rs::Result<bool, std::string> query_attribute(AttributeName name, std::uint32_t mask,
                                                  AnimationAttribute& output) const;
    rs::Result<bool, std::string> evaluate(Evaluation parameters, bool enabled, std::vector<PoseCommand>& output);
};

rs::Result<rs::Unit, std::string> normalize(Slice<float> weights);

}  // namespace s3::animation::playback_tree::blend_space

// Completes PlaybackTree for includers of this header (no-op when already in progress).
#include "s3/core/animation/playback_tree.hpp"
