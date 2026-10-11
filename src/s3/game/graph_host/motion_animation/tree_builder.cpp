// Port of skate-game/src/graph_host/motion_animation/tree_builder.rs.
#include "s3/game/graph_host/motion_animation/tree_builder.hpp"

#include <algorithm>

#include "s3/core/animation/phase_blend.hpp"
#include "s3/core/animation/playback_clip.hpp"
#include "s3/core/animation/playback_tree/blend_space.hpp"
#include "s3/core/animation/playback_tree/selection_space.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/data/result.hpp"

namespace s3::game::graph_host::motion_animation::tree_builder {

using s3::animation::Box;
using s3::animation::output::attributes::AttributeName;
using s3::animation::playback_clip::ClipAttribute;
using s3::animation::playback_clip::PlaybackClip;
using s3::animation::playback_tree::PlaybackTree;
using s3::animation::skeleton_input::name::encode;

namespace {
std::vector<float> floats(const std::vector<std::uint32_t>& bits) {
    std::vector<float> out;
    out.reserve(bits.size());
    for (const std::uint32_t v : bits) out.push_back(rs::from_bits(v));
    return out;
}

using Construction = s3::animation::Slice<const std::pair<AttributeName, AttributeName>>;

rs::Result<std::vector<PlaybackTree>, std::string> build_children(const s3::data::AnimationMetadata& metadata,
                                                                  const std::vector<std::string>& children,
                                                                  Construction construction,
                                                                  std::vector<std::string>& parents) {
    std::vector<PlaybackTree> out;
    out.reserve(children.size());
    for (const std::string& child : children) {
        auto built = build(metadata, child, construction, parents);
        if (built.is_err()) return rs::make_err(built.unwrap_err());
        out.push_back(std::move(built.unwrap()));
    }
    return out;
}
}  // namespace

rs::Result<PlaybackTree, std::string> build(const s3::data::AnimationMetadata& metadata, std::string_view name,
                                            Construction construction, std::vector<std::string>& parents) {
    const std::string key = s3::data::to_ascii_uppercase(std::string(name));
    if (std::find(parents.begin(), parents.end(), key) != parents.end()) {
        return rs::make_err("Cyclic authored animation tree " + key);
    }
    parents.push_back(key);
    auto tree = metadata.tree(std::string(name));
    if (!tree.ok) return rs::make_err(tree.error);
    std::optional<PlaybackTree> result;
    if (const auto* clip = std::get_if<const s3::data::ClipMetadata*>(&tree.value)) {
        const s3::data::ClipMetadata& source = **clip;
        std::vector<ClipAttribute> attributes;
        attributes.reserve(source.attributes.size());
        for (const auto& a : source.attributes) {
            attributes.push_back(ClipAttribute{encode(a.name), a.type_id, rs::from_bits(a.begin_bits),
                                               rs::from_bits(a.end_bits), a.payload_words});
        }
        result = PlaybackTree::Clip{
            source.name,
            PlaybackClip::new_(rs::from_bits(source.frames_bits), rs::from_bits(source.fps_bits),
                               rs::from_bits(source.base_speed_bits), source.flags_word, std::move(attributes))};
    } else if (const auto* space = std::get_if<const s3::data::BlendSpaceMetadata*>(&tree.value)) {
        using s3::animation::playback_tree::blend_space::BlendSpace;
        using s3::animation::playback_tree::blend_space::Simplex;
        const s3::data::BlendSpaceMetadata& source = **space;
        std::vector<AttributeName> parameters;
        for (const std::string& p : source.parameters) parameters.push_back(encode(p));
        auto children = build_children(metadata, source.children, construction, parents);
        if (children.is_err()) return rs::make_err(children.unwrap_err());
        std::vector<Simplex> simplexes;
        for (const auto& s : source.simplexes) {
            Simplex simplex;
            simplex.children = s.children;
            for (const auto& v : s.vertex_bits) simplex.vertices.push_back(floats(v));
            for (const auto& v : s.normal_bits) simplex.normals.push_back(floats(v));
            simplex.scales = floats(s.scale_bits);
            simplexes.push_back(std::move(simplex));
        }
        auto built = BlendSpace::new_(std::move(parameters), std::move(children.unwrap()), std::move(simplexes));
        if (built.is_err()) return rs::make_err(built.unwrap_err());
        result = PlaybackTree::BlendSpace{std::move(built.unwrap())};
    } else if (const auto* blend = std::get_if<const s3::data::PhaseBlendMetadata*>(&tree.value)) {
        const s3::data::PhaseBlendMetadata& source = **blend;
        auto children = build_children(metadata, source.children, construction, parents);
        if (children.is_err()) return rs::make_err(children.unwrap_err());
        auto built = s3::animation::phase_blend::PhaseBlend::new_(encode(source.parameter),
                                                                  std::move(children.unwrap()));
        if (built.is_err()) return rs::make_err(built.unwrap_err());
        result = PlaybackTree::PhaseBlend{std::move(built.unwrap())};
    } else if (const auto* selection = std::get_if<const s3::data::SelectionSpaceMetadata*>(&tree.value)) {
        using s3::animation::playback_tree::selection_space::Candidate;
        using s3::animation::playback_tree::selection_space::Parameter;
        using s3::animation::playback_tree::selection_space::SelectionSpace;
        const s3::data::SelectionSpaceMetadata& source = **selection;
        std::vector<Parameter> parameters;
        for (const auto& p : source.parameters) {
            parameters.push_back(Parameter{encode(p.name), p.mode, rs::from_bits(p.weight_bits),
                                           rs::from_bits(p.minimum_bits), rs::from_bits(p.maximum_bits)});
        }
        std::vector<Candidate> candidates;
        for (const auto& c : source.candidates) {
            auto child = build(metadata, c.child, construction, parents);
            if (child.is_err()) return rs::make_err(child.unwrap_err());
            candidates.push_back(Candidate{c.child, floats(c.value_bits), Box<PlaybackTree>(std::move(child.unwrap()))});
        }
        auto built = SelectionSpace::new_(std::move(parameters), std::move(candidates));
        if (built.is_err()) return rs::make_err(built.unwrap_err());
        result = PlaybackTree::SelectionSpace{std::move(built.unwrap())};
    } else {
        //82D1B9B4..BAD8 resolves the first matching construction value at
        //creation. A missing key/value selects child0; this is authored data.
        const s3::data::SelectorMetadata& source = *std::get<const s3::data::SelectorMetadata*>(tree.value);
        const AttributeName parameter = encode(source.parameter);
        std::optional<AttributeName> value;
        for (const auto& entry : construction) {
            if (entry.first == parameter) {
                value = entry.second;
                break;
            }
        }
        std::string_view child = source.default_;
        for (std::size_t index = 0; index < source.values.size(); ++index) {
            if (value && encode(source.values[index]) == *value) {
                if (index >= source.children.size()) S3_PANIC("index out of bounds");
                child = source.children[index];
                break;
            }
        }
        auto built = build(metadata, child, construction, parents);
        if (built.is_err()) return rs::make_err(built.unwrap_err());
        result = std::move(built.unwrap());
    }
    parents.pop_back();
    return std::move(*result);
}

}  // namespace s3::game::graph_host::motion_animation::tree_builder
