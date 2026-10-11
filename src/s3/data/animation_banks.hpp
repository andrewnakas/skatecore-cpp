// Own both stock skater banks once (animation_banks.rs) and their decoded samples
// (animation_frames.rs + animation_frames/native.rs).
// Clip/tree names are disjoint; poses retain their bank identity because the banks
// share several reference-pose names. Decoded stock samples: playback/interpolation and
// reference composition are separate core operations; this loader never repairs missing
// bone channels.
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "abin.hpp"
#include "animation_metadata.hpp"
#include "result.hpp"
#include "vfs.hpp"

namespace s3::data {

class AnimationBanks
{
public:
	std::vector<std::shared_ptr<abin::Bank>> banks;
	std::vector<std::string> identities; // SHA-256 hex of each bank

	// `<asset_root>/private/stock/data/anim/{OnBoard,OffBoard}.abin`
	static Result<AnimationBanks> load(const Vfs &vfs, const std::string &asset_root);
	Result<AnimationMetadata> metadata() const;
};

/// Scale xyz, quaternion xyzw, translation xyz; each component is an f32 word.
using SampleWords = std::array<uint32_t, 10>;

struct ClipFrames
{
	std::string name;
	uint64_t source_offset = 0;
	uint32_t fps_bits = 0;
	std::array<uint32_t, 3> loop_translation_bits{};
	std::array<uint32_t, 4> loop_rotation_bits{};
	bool channel_animation = false;
	std::vector<uint32_t> channel_weights;
	std::vector<std::vector<SampleWords>> frames;
};

struct ReferencePose
{
	std::string name;
	uint64_t source_offset = 0;
	std::vector<SampleWords> samples;
};

namespace animation_frames_native {
//! Bank-backed lazy animation samples. Each requested clip is decoded once;
//! registration indexes every record without allocating every decoded frame.
//! (Lazy decode is not thread-safe; decode from one thread or pre-warm.)
template <class T>
struct Entry
{
	size_t bank = 0;
	size_t offset = 0;
	mutable std::unique_ptr<Result<T>> decoded;
};
class NativeFrames
{
public:
	static Result<NativeFrames> create(const AnimationBanks &banks);
	size_t clip_count() const { return clips_.size(); }
	std::vector<std::string> clip_names() const;
	Result<size_t> clip_bank(const std::string &name) const;
	Result<const ClipFrames *> clip(const std::string &name) const;
	Result<const ReferencePose *> named_pose(size_t bank, const std::string &name) const;
	Result<const ReferencePose *> reference_pose(size_t bank, uint64_t offset) const;

private:
	std::vector<std::shared_ptr<abin::Bank>> banks_;
	std::map<std::string, Entry<ClipFrames>> clips_;
	std::map<std::pair<size_t, uint64_t>, Entry<ReferencePose>> poses_;
	std::map<std::pair<size_t, std::string>, uint64_t> pose_names_;
};
} // namespace animation_frames_native

class AnimationFrames
{
public:
	std::string source_sha256;
	std::string decoder_sha256;
	bool decoder_hardware_verified = false;
	std::vector<int32_t> parents;
	std::vector<std::string> bone_names;
	std::vector<int32_t> mirror_indices;
	bool has_trajectory = false;

	static Result<AnimationFrames> from_banks(const AnimationBanks &banks);
	static Result<AnimationFrames> load(const Vfs &vfs, const std::string &path, const AnimationMetadata &metadata);
	static Result<AnimationFrames> parse(std::string_view text, const AnimationMetadata &metadata);

	Result<const ClipFrames *> clip(const std::string &name) const;
	/// Explicit source-record lookup, useful when auditing original bank data.
	Result<const ReferencePose *> reference_pose(uint64_t source_offset) const;
	/// SetDBContent82D1B45C assigns into the selected database's name map,
	/// replacing earlier same-name records in that bank.
	Result<const ReferencePose *> named_pose(const std::string &name) const { return named_pose_in(0, name); }
	Result<const ReferencePose *> named_pose_in(size_t bank, const std::string &name) const;
	size_t clip_count() const;
	std::vector<std::string> clip_names() const;
	Result<size_t> clip_bank(const std::string &name) const;

private:
	std::map<std::string, ClipFrames> clips_;
	std::map<uint64_t, ReferencePose> reference_poses_;
	std::shared_ptr<animation_frames_native::NativeFrames> native_;
};

Status validate_samples(const std::vector<SampleWords> &samples, size_t bones);

} // namespace s3::data
