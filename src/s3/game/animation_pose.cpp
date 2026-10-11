// Port of skate-game/src/animation_pose.rs.
#include "s3/game/animation_pose.hpp"

#include <mutex>
#include <optional>
#include <type_traits>

#include "s3/core/animation/pose_add.hpp"
#include "s3/core/animation/pose_blend.hpp"
#include "s3/core/animation/pose_mirror.hpp"
#include "s3/core/animation/pose_sample.hpp"
#include "s3/core/animation/pose_trajectory.hpp"
#include "s3/game/graph_runtime/disk.hpp"

namespace s3::game::animation_pose {

namespace pose_add = s3::animation::pose_add;
namespace pose_blend = s3::animation::pose_blend;
namespace pose_mirror = s3::animation::pose_mirror;
namespace pose_trajectory = s3::animation::pose_trajectory;
using s3::animation::output::PoseBufferError;
using s3::animation::pose_sample::sample_key;
using s3::animation::pose_sample::select_frames;
using s3::animation::pose_trajectory::LoopTransform;

namespace {

/// Rust `{:?}` of `PoseBufferError`.
std::string debug_error(const PoseBufferError& e) {
    return std::visit(
        [](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, PoseBufferError::NonFiniteWeight>) return "NonFiniteWeight";
            if constexpr (std::is_same_v<T, PoseBufferError::InvalidQuaternion>) return "InvalidQuaternion";
            if constexpr (std::is_same_v<T, PoseBufferError::CountOverflow>) return "CountOverflow";
            if constexpr (std::is_same_v<T, PoseBufferError::ShortInput>) return "ShortInput";
            if constexpr (std::is_same_v<T, PoseBufferError::ShortOutput>) return "ShortOutput";
            if constexpr (std::is_same_v<T, PoseBufferError::ShortParents>) return "ShortParents";
            if constexpr (std::is_same_v<T, PoseBufferError::InvalidCacheOffset>) return "InvalidCacheOffset";
            if constexpr (std::is_same_v<T, PoseBufferError::InvalidParent>)
                return "InvalidParent { bone: " + std::to_string(v.bone) + ", parent: " + std::to_string(v.parent) +
                       " }";
            return "";
        },
        e.value);
}

template <class R>
std::string err_string(const R& r) {
    return std::string(r.unwrap_err());
}

}  // namespace

rs::Result<PoseEvaluator, std::string> PoseEvaluator::load(const s3::data::Vfs& vfs, const std::string& asset_root) {
    auto banks = AnimationBanks::load(vfs, asset_root);
    if (!banks.ok) return rs::make_err(banks.error);
    auto evaluator = from_banks(banks.value);
    if (evaluator.is_err()) return rs::make_err(evaluator.unwrap_err());
    auto loaded = evaluator.unwrap().load_authored_clips(vfs, asset_root);
    if (loaded.is_err()) return rs::make_err(loaded.unwrap_err());
    return std::move(evaluator.unwrap());
}

rs::Result<PoseEvaluator, std::string> PoseEvaluator::load(const std::string& asset_root) {
    namespace disk = graph_runtime::disk;
    const s3::data::Vfs vfs =
        disk::read_files(asset_root, {disk::ON_BOARD_BANK, disk::OFF_BOARD_BANK, disk::AUTHORED_CLIPS});
    return load(vfs, asset_root);
}

rs::Result<PoseEvaluator, std::string> PoseEvaluator::from_banks(const AnimationBanks& banks) {
    auto frames = AnimationFrames::from_banks(banks);
    if (!frames.ok) return rs::make_err(frames.error);
    return PoseEvaluator(std::move(frames.value));
}

rs::Result<rs::Unit, std::string> PoseEvaluator::load_authored_clips(const s3::data::Vfs& vfs,
                                                                     const std::string& root) {
    auto replacements = authored_clips::Replacements::load(vfs, root, frames);
    if (replacements.is_err()) return rs::make_err(replacements.unwrap_err());
    authored_ = std::move(replacements.unwrap());
    return rs::Unit{};
}

rs::Result<rs::Unit, std::string> PoseEvaluator::load_authored_clips(const std::string& root) {
    auto replacements = authored_clips::Replacements::load(root, frames);
    if (replacements.is_err()) return rs::make_err(replacements.unwrap_err());
    authored_ = std::move(replacements.unwrap());
    return rs::Unit{};
}

rs::Result<rs::Unit, std::string> PoseEvaluator::install_mod_clips(std::string_view owner,
                                                                   std::string_view text) const {
    auto clips = authored_clips::Replacements::parse(text, frames);
    if (clips.is_err()) return rs::make_err(clips.unwrap_err());
    std::unique_lock<std::shared_mutex> lock(*mod_clips_lock_);
    for (const auto& [id, other] : mod_clips_) {
        if (id != owner) {
            for (const auto& entry : clips.unwrap()._0) {
                if (other._0.count(entry.first) != 0) {
                    return rs::make_err("Animation slots conflict with mod " + id);
                }
            }
        }
    }
    mod_clips_[std::string(owner)] = std::move(clips.unwrap());
    return rs::Unit{};
}

void PoseEvaluator::remove_mod_clips(std::string_view owner) const {
    std::unique_lock<std::shared_mutex> lock(*mod_clips_lock_);
    mod_clips_.erase(std::string(owner));
}

void PoseEvaluator::clear_mod_clips() const {
    std::unique_lock<std::shared_mutex> lock(*mod_clips_lock_);
    mod_clips_.clear();
}

rs::Result<std::vector<Sqt>, std::string> PoseEvaluator::evaluate(
    s3::animation::Slice<const PoseCommand> commands) const {
    std::shared_lock<std::shared_mutex> lock(*mod_clips_lock_);
    std::vector<std::vector<Sqt>> stack;
    for (const PoseCommand& command : commands) {
        if (const auto* c = std::get_if<PoseCommand::Pose>(&command.value)) {
            // Init82B97E38 retains these references from the actor's
            // initial database. AddBindPose82B98118 reuses them even
            // when a later motion clip comes from another bank.
            auto pose = frames.named_pose(c->name);
            if (!pose.ok) return rs::make_err(pose.error);
            std::vector<Sqt> samples;
            samples.reserve(pose.value->samples.size());
            for (const SampleWords& words : pose.value->samples) samples.push_back(sqt(words));
            stack.push_back(std::move(samples));
        } else if (const auto* c = std::get_if<PoseCommand::Add>(&command.value)) {
            if (stack.empty()) return rs::make_err(std::string("Animation add has no reference subtree"));
            std::vector<Sqt> other = std::move(stack.back());
            stack.pop_back();
            if (stack.empty()) return rs::make_err(std::string("Animation add has no motion subtree"));
            std::vector<Sqt>& motion = stack.back();
            if (motion.size() != other.size()) return rs::make_err(std::string("Animation add bone counts differ"));
            for (std::size_t i = 0; i < motion.size(); ++i) {
                motion[i] = c->motion_is_a ? pose_add::add(motion[i], other[i], true)
                                           : pose_add::add(other[i], motion[i], false);
            }
        } else if (const auto* c = std::get_if<PoseCommand::Mirror>(&command.value)) {
            if (stack.empty()) return rs::make_err(std::string("Animation mirror has no subtree"));
            std::vector<Sqt>& pose = stack.back();
            auto mirrored = pose_mirror::mirror(pose, frames.parents, frames.mirror_indices, c->trajectory_mode);
            if (mirrored.is_err()) return rs::make_err(err_string(mirrored));
        } else if (const auto* c = std::get_if<PoseCommand::Clip>(&command.value)) {
            auto stock_result = frames.clip(c->name);
            if (!stock_result.ok) return rs::make_err(stock_result.error);
            const ClipFrames* stock = stock_result.value;
            const ClipFrames* clip = nullptr;
            for (const auto& entry : mod_clips_) {
                if ((clip = entry.second.clip(stock->name)) != nullptr) break;
            }
            if (clip == nullptr) clip = authored_.clip(stock->name);
            if (clip == nullptr) clip = stock;
            auto sampled = sample_clip(*clip, c->time);
            if (sampled.is_err()) return rs::make_err(sampled.unwrap_err());
            std::vector<Sqt> pose = std::move(sampled.unwrap());
            if (frames.has_trajectory) {
                auto previous = sample_bone(*clip, c->previous_time, 0);
                if (previous.is_err()) return rs::make_err(previous.unwrap_err());
                std::optional<LoopTransform> loop_transform;
                if (c->loops != 0) {
                    LoopTransform t;
                    t.rotation = rs::from_bits(clip->loop_rotation_bits);
                    t.translation = rs::from_bits(clip->loop_translation_bits);
                    loop_transform = t;
                }
                pose[0] = pose_trajectory::delta(pose[0], previous.unwrap(), loop_transform);
            }
            stack.push_back(std::move(pose));
        } else if (const auto* c = std::get_if<PoseCommand::WeightedBlend>(&command.value)) {
            if (stack.size() < c->weights.size())
                return rs::make_err(std::string("Weighted blend pose stack underflow"));
            const std::size_t start = stack.size() - c->weights.size();
            auto pose = pose_blend::weighted(
                s3::animation::Slice<const std::vector<Sqt>>(stack.data() + start, stack.size() - start),
                c->weights);
            if (pose.is_err()) return rs::make_err("Weighted blend: " + debug_error(pose.unwrap_err()));
            stack.resize(start);
            stack.push_back(std::move(pose.unwrap()));
        } else {
            // PoseCommand::Blend { weight } | PoseCommand::ChannelBlend { weight, .. }
            if (stack.empty()) return rs::make_err(std::string("Animation blend has no second subtree"));
            std::vector<Sqt> second = std::move(stack.back());
            stack.pop_back();
            if (stack.empty()) return rs::make_err(std::string("Animation blend has no first subtree"));
            std::vector<Sqt>& first = stack.back();
            if (first.size() != second.size())
                return rs::make_err(std::string("Animation subtree bone counts differ"));
            const auto* channel = std::get_if<PoseCommand::ChannelBlend>(&command.value);
            const float weight = channel ? channel->weight : std::get<PoseCommand::Blend>(command.value).weight;
            for (std::size_t i = 0; i < first.size(); ++i) {
                first[i] = channel ? pose_blend::channel_blend_sample(first[i], second[i], weight,
                                                                      channel->use_channels_from_weights)
                                   : pose_blend::blend_sample(first[i], second[i], weight);
            }
        }
    }
    if (stack.size() != 1) {
        return rs::make_err("Animation evaluation left" + std::to_string(stack.size()) + " poses");
    }
    std::vector<Sqt> out = std::move(stack.back());
    return out;
}

rs::Result<std::vector<NativeMatrix>, std::string> PoseEvaluator::hierarchy(
    s3::animation::Slice<const Sqt> pose) const {
    if (pose.size() != frames.parents.size()) {
        return rs::make_err(std::string("Animation pose and hierarchy bone counts differ"));
    }
    std::vector<NativeMatrix> globals;
    globals.reserve(pose.size());
    for (const Sqt& s : pose) globals.push_back(s3::animation::output::sqt_to_matrix(s));
    auto composed = s3::animation::output::compose_hierarchy_in_place(static_cast<std::int32_t>(globals.size()),
                                                                     frames.parents, 0, globals);
    if (composed.is_err()) {
        return rs::make_err("Invalid stock animation hierarchy: " + debug_error(composed.unwrap_err()));
    }
    return globals;
}

rs::Result<std::vector<Sqt>, std::string> sample_clip(const ClipFrames& clip, float time) {
    auto selection_result = select_frames(time, rs::from_bits(clip.fps_bits), clip.frames.size(), true, 0.0f);
    if (selection_result.is_err()) return rs::make_err(err_string(selection_result));
    const auto selection = selection_result.unwrap();
    const auto& first = clip.frames[selection.first];
    const auto& second = clip.frames[selection.second];
    std::vector<Sqt> out;
    const std::size_t n = first.size() < second.size() ? first.size() : second.size();
    out.reserve(n);
    for (std::size_t bone = 0; bone < n; ++bone) {
        // ClipFrames already contains independently decoded keys. The raw
        // VBR block-boundary compatibility path substitutes the next key,
        // which skips then holds a frame when applied to this flat cache.
        Sqt sample = sample_key(sqt(first[bone]), sqt(second[bone]), selection);
        sample.translation[3] = rs::from_bits(clip.channel_weights.at(bone));
        out.push_back(sample);
    }
    return out;
}

rs::Result<Sqt, std::string> sample_bone(const ClipFrames& clip, float time, std::size_t bone) {
    auto selection_result = select_frames(time, rs::from_bits(clip.fps_bits), clip.frames.size(), true, 0.0f);
    if (selection_result.is_err()) return rs::make_err(err_string(selection_result));
    const auto selection = selection_result.unwrap();
    Sqt sample = sample_key(sqt(clip.frames[selection.first].at(bone)), sqt(clip.frames[selection.second].at(bone)),
                            selection);
    sample.translation[3] = rs::from_bits(clip.channel_weights.at(bone));
    return sample;
}

Sqt sqt(const SampleWords& words) {
    const auto v = rs::from_bits(words);
    Sqt out;
    out.scale = {v[0], v[1], v[2], 1.0f};
    out.rotation = {v[3], v[4], v[5], v[6]};
    out.translation = {v[7], v[8], v[9], 1.0f};
    return out;
}

}  // namespace s3::game::animation_pose
