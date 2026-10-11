// Port of skate-game/src/graph_host/pushing_settings.rs.
//! Stock pushing settings and authored clip metadata (TU382B953E8..82B9584C).
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "s3/core/riding/push_animation.hpp"
#include "s3/core/riding/push_behaviors.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/animation_metadata.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/vfs.hpp"

namespace s3::game::graph_host::pushing_settings {

using s3::riding::push_animation::PushAnimationCurves;
using s3::riding::push_behaviors::PushAttributes;
using s3::riding::push_behaviors::PushClipMetrics;

/// A tree's ordered attributes. The value is its first payload lane, including
/// non-scalar types: native82B95620 does not filter the attribute type.
struct PushTreeAttribute {
    std::string name;
    float first_payload = 0.0f;
};
struct PushTreeMetadata {
    float length = 0.0f;
    std::vector<PushTreeAttribute> attributes;
};

struct PushTreeSource {
    virtual ~PushTreeSource() = default;
    /// Obtain the actual animation tree's length and GetAttributes(mask=15)
    /// result in stored order. Missing assets must return an error.
    virtual rs::Result<PushTreeMetadata, std::string> metadata(std::string_view tree_name,
                                                               std::uint32_t attribute_mask) = 0;
};

/// Rust `impl PushTreeSource for AnimationMetadata` as a free function.
rs::Result<PushTreeMetadata, std::string> metadata(const s3::data::AnimationMetadata& self,
                                                   std::string_view tree_name, std::uint32_t attribute_mask);

/// Adapter for `impl PushTreeSource for AnimationMetadata` (C++ cannot add the trait to the
/// s3::data type itself).
struct AnimationMetadataTrees : PushTreeSource {
    const s3::data::AnimationMetadata& trees;
    explicit AnimationMetadataTrees(const s3::data::AnimationMetadata& trees) : trees(trees) {}
    rs::Result<PushTreeMetadata, std::string> metadata(std::string_view tree_name,
                                                       std::uint32_t attribute_mask) override {
        return pushing_settings::metadata(trees, tree_name, attribute_mask);
    }
};

struct PushingSettings {
    PushAnimationCurves curves;
    float teleport_window = 0.0f;
    float maximum_holding_acceleration = 0.0f;
    float out_speed_weight = 0.0f;
    float maximum_out_factor = 0.0f;
    PushAttributes regular;
    PushAttributes mongo;

    /// Standalone settings check; game startup shares its already loaded banks.
    /// `asset_root` is read from the filesystem (Rust `&Path`).
    static rs::Result<PushingSettings, std::string> load_stock(const s3::data::Collections& data,
                                                                const std::string& asset_root);
    /// Same, reading the banks from an existing Vfs.
    static rs::Result<PushingSettings, std::string> load_stock(const s3::data::Collections& data,
                                                                const s3::data::Vfs& vfs,
                                                                const std::string& asset_root);
    static rs::Result<PushingSettings, std::string> load(const s3::data::Collections& data, PushTreeSource& trees);
    /// TU382BAD3B4..3DC/82BADBB8..BE0; GetIsSwitch, not natural stance.
    const PushAttributes& attributes(bool regular_attributes, bool is_switch) const {
        return regular_attributes != is_switch ? regular : mongo;
    }
};

}  // namespace s3::game::graph_host::pushing_settings
