// Port of skate-game/src/physics/climbing/clip.rs
//! Optional authored clips, separate from the SHA-bound stock animation banks.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "s3/data/vfs.hpp"
#include "s3/game/animation_pose/glam.hpp"
#include "s3/game/common.hpp"
#include "s3/game/transform_shim.hpp"

namespace s3::game::physics::climbing::clip {

using glam::Mat4;
using glam::Quat;
using glam::Vec3;
using transform_shim::Transform;

/// `#[derive(Deserialize)] struct Clip`.
struct Clip {
    std::string name;
    float fps = 0.0f;
    std::vector<std::string> names;
    std::vector<std::int32_t> parents;
    std::vector<std::vector<std::array<float, 10>>> frames;

    rs::Result<rs::Unit, std::string> validate() const;
    float duration() const;
    std::vector<Transform> sample(float time) const;
    std::vector<Mat4> globals(const std::vector<Transform>& locals) const;
    std::size_t index(std::string_view name) const;
    Vec3 hands(const std::vector<Mat4>& globals) const;
    Vec3 feet(const std::vector<Mat4>& globals) const;
};

/// `#[derive(Deserialize)] struct File` (serde_json).
struct File {
    std::uint32_t version = 0;
    std::vector<Clip> clips;
};

struct Clips {
    Clip reach;
    Clip mantle;

    /// `root: &Path` -> (vfs, asset root); a missing `private/custom/climbing.json` is `Ok(None)`.
    static rs::Result<std::optional<Clips>, std::string> load(const sd::Vfs& vfs, const std::string& root);
};

Transform decode(const std::array<float, 10>& s);
Transform blend(Transform a, Transform b, float t);

}  // namespace s3::game::physics::climbing::clip
