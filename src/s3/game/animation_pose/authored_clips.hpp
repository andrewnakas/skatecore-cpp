// Port of skate-game/src/animation_pose/authored_clips.rs.
//! Optional authored body clips. Stock graph timing, board, trajectory and attributes remain authoritative.
#pragma once
#include <map>
#include <string>
#include <string_view>

#include "s3/core/animation/output.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/animation_pose/glam.hpp"

namespace s3::game::animation_pose::authored_clips {

using s3::animation::output::Sqt;
using s3::data::AnimationFrames;
using s3::data::ClipFrames;
using s3::data::SampleWords;

/// Rust tuple struct `Replacements(BTreeMap<String, ClipFrames>)`; `_0` is Rust's `.0`.
struct Replacements {
    std::map<std::string, ClipFrames> _0;

    /// Reads `<root>/private/custom/crouch-treflip.json` from the filesystem (Rust `&Path`).
    /// A missing file yields the empty default.
    static rs::Result<Replacements, std::string> load(const std::string& root, const AnimationFrames& frames);
    /// Same, from a Vfs: a file absent from `vfs` yields the empty default.
    static rs::Result<Replacements, std::string> load(const s3::data::Vfs& vfs, const std::string& root,
                                                      const AnimationFrames& frames);
    static rs::Result<Replacements, std::string> parse(std::string_view text, const AnimationFrames& frames);
    /// nullptr = None.
    const ClipFrames* clip(const std::string& name) const {
        auto it = _0.find(name);
        return it == _0.end() ? nullptr : &it->second;
    }
};

/// Private helpers in Rust (the parent module's `sqt` is in animation_pose.hpp).
glam::Mat4 matrix(const Sqt& s);
Sqt remove_reference(const glam::Mat4& local, const Sqt& reference);
SampleWords words(const Sqt& s);

}  // namespace s3::game::animation_pose::authored_clips
