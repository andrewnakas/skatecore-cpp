// Port of skate-game/src/animation_pose.rs.
//! Production animation command evaluator. File IO and buffer ownership are
//! host services; frame selection, trajectory and pose math live in core.
#pragma once
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include "s3/core/animation/output.hpp"
#include "s3/core/animation/playback_tree.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/animation_pose/authored_clips.hpp"

namespace s3::game::animation_pose {

using s3::animation::output::NativeMatrix;
using s3::animation::output::Sqt;
using s3::animation::playback_tree::PoseCommand;
using s3::data::AnimationBanks;
using s3::data::AnimationFrames;
using s3::data::ClipFrames;
using s3::data::SampleWords;

class PoseEvaluator {
public:
    AnimationFrames frames;

    /// `asset_root` is a filesystem directory (Rust `&Path`): reads both stock banks and the
    /// optional authored clip file from disk.
    static rs::Result<PoseEvaluator, std::string> load(const std::string& asset_root);
    /// Same, from an existing Vfs.
    static rs::Result<PoseEvaluator, std::string> load(const s3::data::Vfs& vfs, const std::string& asset_root);
    static rs::Result<PoseEvaluator, std::string> from_banks(const AnimationBanks& banks);

    rs::Result<rs::Unit, std::string> load_authored_clips(const std::string& root);
    rs::Result<rs::Unit, std::string> load_authored_clips(const s3::data::Vfs& vfs, const std::string& root);

    /// Reuses the existing constrained body-clip parser; never replaces board/trajectory data.
    rs::Result<rs::Unit, std::string> install_mod_clips(std::string_view owner, std::string_view text) const;
    void remove_mod_clips(std::string_view owner) const;
    void clear_mod_clips() const;

    /// Executes the ordered stock tree. This uses the native immediate ACS
    /// arithmetic; the host does not recreate packed animation job commands.
    rs::Result<std::vector<Sqt>, std::string> evaluate(s3::animation::Slice<const PoseCommand> commands) const;

    /// TU3828D3800 then828D3B58. NewACS824744B8 initializes the
    /// detached parent to0, so trajectory deltas do not move the local rig.
    rs::Result<std::vector<NativeMatrix>, std::string> hierarchy(s3::animation::Slice<const Sqt> pose) const;

private:
    explicit PoseEvaluator(AnimationFrames frames_)
        : frames(std::move(frames_)), mod_clips_lock_(std::make_unique<std::shared_mutex>()) {}

    authored_clips::Replacements authored_;
    /// Rust `RwLock<BTreeMap<String, Replacements>>` (interior mutability for `&self`).
    mutable std::map<std::string, authored_clips::Replacements> mod_clips_;
    std::unique_ptr<std::shared_mutex> mod_clips_lock_;
};

/// Private in Rust (used by the parent tests).
rs::Result<std::vector<Sqt>, std::string> sample_clip(const ClipFrames& clip, float time);
rs::Result<Sqt, std::string> sample_bone(const ClipFrames& clip, float time, std::size_t bone);
Sqt sqt(const SampleWords& words);

}  // namespace s3::game::animation_pose
