// Stock skater banks and decoded samples (animation_banks.rs, animation_frames.rs, animation_frames/native.rs).
#include "animation_banks.hpp"

#include <cmath>
#include <set>

#include "attrib_hash.hpp"
#include "json.hpp"
#include "vbr.hpp"

namespace s3::data {

// ---- AnimationBanks ----

Result<AnimationBanks> AnimationBanks::load(const Vfs &vfs, const std::string &asset_root)
{
	AnimationBanks out;
	for (const char *name : {"OnBoard", "OffBoard"}) {
		std::string path = join_path(asset_root, std::string("private/stock/data/anim/") + name + ".abin");
		S3_TRY_MAP(abin::Bank bank, abin::Bank::load(vfs, path), [&](const std::string &e) { return std::string(name) + ": " + e; });
		S3_CHECK(bank.validate_positional_parts());
		out.identities.push_back(sha256::digest(bank.bytes(), bank.size()));
		out.banks.push_back(std::make_shared<abin::Bank>(std::move(bank)));
	}
	const abin::Hierarchy *first = out.banks[0]->hierarchy();
	if (!first) return Error{"OnBoard has no animation hierarchy"};
	const abin::Hierarchy *second = out.banks[1]->hierarchy();
	if (!second) return Error{"OffBoard has no animation hierarchy"};
	if (!first->compatible_with(*second)) return Error{"Skater animation bank hierarchies differ"};
	return out;
}

Result<AnimationMetadata> AnimationBanks::metadata() const
{
	S3_TRY(AnimationMetadata result, AnimationMetadata::from_bank(*banks[0], "OnBoard.abin", identities[0]));
	S3_TRY(AnimationMetadata other, AnimationMetadata::from_bank(*banks[1], "OffBoard.abin", identities[1]));
	S3_CHECK(result.merge(std::move(other)));
	return result;
}

// ---- validate_samples ----

Status validate_samples(const std::vector<SampleWords> &samples, size_t bones)
{
	bool bad = samples.size() != bones;
	if (!bad)
		for (const SampleWords &s : samples)
			for (uint32_t w : s)
				if (!is_finite(f32_from_bits(w))) bad = true;
	if (bad) return Error{"Decoded stock frame is incomplete or nonfinite"};
	return Unit{};
}

// ---- native.rs ----

namespace animation_frames_native {

static Status require_vbr(abin::Codec codec)
{
	if (codec.kind != abin::Codec::Vbr) return Error{"Unsupported native animation codec " + codec.debug()};
	return Unit{};
}

static Result<std::vector<std::vector<SampleWords>>> decode_parts(const abin::Bank &bank, const std::vector<abin::PartEntry> &parts,
                                                                   size_t count)
{
	const abin::Hierarchy *hierarchy = bank.hierarchy();
	if (!hierarchy) return Error{"Missing stock hierarchy"};
	size_t bones = hierarchy->bone_count;
	if (parts.size() != hierarchy->parts.size()) return Error{"Animation part count differs from hierarchy"};
	std::vector<std::vector<SampleWords>> frames(count, std::vector<SampleWords>(bones, SampleWords{}));
	std::vector<bool> assigned(bones, false);
	for (size_t p = 0; p < parts.size(); p++) {
		const abin::PartEntry &entry = parts[p];
		const abin::HierarchyPart &layout = hierarchy->parts[p];
		if (!entry.part) return Error{"Required stock animation part is absent"};
		const abin::AnimationPart &part = *entry.part;
		S3_TRY(vbr::VbrDecoder decoder, vbr::VbrDecoder::create(bank.bytes(), bank.size(), part.offset, part.compression_header_relative,
		                                                        part.compressed_data_relative));
		if (decoder.frame_count() != count || decoder.channel_count() != (size_t)layout.bone_count)
			return Error{layout.name + ": codec frame/channel count disagrees with bank header"};
		if (layout.sqt_offset < 0) return Error{"Negative animation part offset"};
		size_t start = (size_t)layout.sqt_offset;
		size_t end = start + layout.bone_count;
		bool overlap = end > bones;
		if (!overlap)
			for (size_t i = start; i < end; i++) overlap |= assigned[i];
		if (overlap) return Error{"Overlapping animation parts"};
		for (size_t i = start; i < end; i++) assigned[i] = true;
		for (size_t index = 0; index < frames.size(); index++) {
			S3_TRY(std::vector<SampleWords> decoded, decoder.decode_frame(index));
			if (decoded.size() != end - start) return Error{"Decoded part size mismatch"};
			for (size_t i = 0; i < decoded.size(); i++) frames[index][start + i] = decoded[i];
		}
	}
	for (bool v : assigned)
		if (!v) return Error{"Incomplete animation hierarchy channels"};
	for (const auto &frame : frames) S3_CHECK(validate_samples(frame, bones));
	return frames;
}

static Result<ClipFrames> decode_clip(const abin::Bank &bank, size_t offset)
{
	const abin::Record *record = bank.record_at(offset);
	if (!record) return Error{"Missing clip source record"};
	const abin::Clip *clip = std::get_if<abin::Clip>(&record->data);
	if (!clip) return Error{"Source record is not a clip"};
	S3_CHECK(require_vbr(record->header.codec()));
	float count = f32_from_bits(clip->frame_count_bits);
	if (!is_finite(count) || count < 1.0f || count - std::trunc(count) != 0.0f) return Error{record->header.name + ": invalid frame count"};
	S3_TRY_MAP(auto frames, decode_parts(bank, clip->parts, (size_t)count),
	           [&](const std::string &e) { return record->header.name + ": " + e; });
	const abin::Hierarchy *hierarchy = bank.hierarchy();
	if (!hierarchy) return Error{"No bank hierarchy"};
	size_t bones = hierarchy->bone_count;
	std::vector<uint32_t> weights(bones, f32_to_bits(1.0f));
	if (clip->channel_animation()) {
		abin::Reader reader(bank.bytes(), bank.size());
		size_t n = std::min(clip->parts.size(), hierarchy->parts.size());
		for (size_t p = 0; p < n; p++) {
			const abin::PartEntry &entry = clip->parts[p];
			const abin::HierarchyPart &layout = hierarchy->parts[p];
			if (!entry.part) return Error{"Channel animation part is absent"};
			if (!entry.part->channel_weights) return Error{"Channel animation weights are absent"};
			const abin::Range &range = *entry.part->channel_weights;
			for (size_t lane = 0; lane < layout.bone_count; lane++) {
				S3_TRY(uint32_t word, reader.u32(range.start + lane * 4));
				if (!is_finite(f32_from_bits(word))) return Error{"Nonfinite channel weight"};
				weights[(size_t)layout.sqt_offset + lane] = word;
			}
		}
	}
	ClipFrames out;
	out.name = record->header.name;
	out.source_offset = offset;
	out.fps_bits = clip->fps_bits;
	out.loop_translation_bits = {clip->loop_translation_words[0], clip->loop_translation_words[1], clip->loop_translation_words[2]};
	out.loop_rotation_bits = clip->loop_rotation_words;
	out.channel_animation = clip->channel_animation();
	out.channel_weights = std::move(weights);
	out.frames = std::move(frames);
	return out;
}

static Result<ReferencePose> decode_pose(const abin::Bank &bank, size_t offset)
{
	const abin::Record *record = bank.record_at(offset);
	if (!record) return Error{"Missing pose source record"};
	const abin::Pose *pose = std::get_if<abin::Pose>(&record->data);
	if (!pose) return Error{"Source record is not a pose"};
	S3_CHECK(require_vbr(record->header.codec()));
	S3_TRY_MAP(auto frames, decode_parts(bank, pose->parts, 1), [&](const std::string &e) { return record->header.name + ": " + e; });
	ReferencePose out;
	out.name = record->header.name;
	out.source_offset = offset;
	out.samples = std::move(frames[0]);
	return out;
}

Result<NativeFrames> NativeFrames::create(const AnimationBanks &banks)
{
	NativeFrames out;
	for (size_t bank_index = 0; bank_index < banks.banks.size(); bank_index++) {
		for (const abin::Record &record : banks.banks[bank_index]->records()) {
			const std::string &name = record.header.name;
			size_t at = record.header.offset;
			if (std::holds_alternative<abin::Clip>(record.data)) {
				auto it = out.clips_.find(name);
				if (it != out.clips_.end() && it->second.bank != bank_index)
					return Error{"Ambiguous clip " + name + " in multiple animation banks"};
				Entry<ClipFrames> &e = out.clips_[name];
				e.bank = bank_index;
				e.offset = at;
				e.decoded.reset();
			} else if (std::holds_alternative<abin::Pose>(record.data)) {
				Entry<ReferencePose> &e = out.poses_[{bank_index, (uint64_t)at}];
				e.bank = bank_index;
				e.offset = at;
				e.decoded.reset();
				out.pose_names_[{bank_index, name}] = at;
			}
		}
	}
	out.banks_ = banks.banks;
	return out;
}

std::vector<std::string> NativeFrames::clip_names() const
{
	std::vector<std::string> out;
	for (auto &kv : clips_) out.push_back(kv.first);
	return out;
}

Result<size_t> NativeFrames::clip_bank(const std::string &name) const
{
	auto it = clips_.find(to_ascii_uppercase(name));
	if (it == clips_.end()) return Error{"Missing stock animation clip " + name};
	return it->second.bank;
}

Result<const ClipFrames *> NativeFrames::clip(const std::string &name) const
{
	auto it = clips_.find(to_ascii_uppercase(name));
	if (it == clips_.end()) return Error{"Missing stock animation clip " + name};
	const Entry<ClipFrames> &entry = it->second;
	if (!entry.decoded) entry.decoded = std::make_unique<Result<ClipFrames>>(decode_clip(*banks_[entry.bank], entry.offset));
	if (!entry.decoded->ok) return Error{entry.decoded->error};
	return &entry.decoded->value;
}

Result<const ReferencePose *> NativeFrames::named_pose(size_t bank, const std::string &name) const
{
	auto it = pose_names_.find({bank, to_ascii_uppercase(name)});
	if (it == pose_names_.end()) return Error{"Missing reference pose " + name + " in bank " + std::to_string(bank)};
	return reference_pose(bank, it->second);
}

Result<const ReferencePose *> NativeFrames::reference_pose(size_t bank, uint64_t offset) const
{
	auto it = poses_.find({bank, offset});
	if (it == poses_.end()) return Error{"Missing reference pose " + std::to_string(bank) + ":" + hex_prefixed(offset)};
	const Entry<ReferencePose> &entry = it->second;
	if (!entry.decoded) entry.decoded = std::make_unique<Result<ReferencePose>>(decode_pose(*banks_[bank], entry.offset));
	if (!entry.decoded->ok) return Error{entry.decoded->error};
	return &entry.decoded->value;
}

} // namespace animation_frames_native

// ---- AnimationFrames ----

Result<AnimationFrames> AnimationFrames::from_banks(const AnimationBanks &banks)
{
	const abin::Hierarchy *hierarchy = banks.banks[0]->hierarchy();
	if (!hierarchy) return Error{"Missing animation hierarchy"};
	AnimationFrames out;
	out.source_sha256 = banks.identities[0];
	out.decoder_hardware_verified = false;
	out.parents = hierarchy->parents;
	out.bone_names = hierarchy->bone_names;
	out.mirror_indices = hierarchy->mirrors;
	out.has_trajectory = hierarchy->has_trajectory;
	S3_TRY(animation_frames_native::NativeFrames native, animation_frames_native::NativeFrames::create(banks));
	out.native_ = std::make_shared<animation_frames_native::NativeFrames>(std::move(native));
	return out;
}

Result<AnimationFrames> AnimationFrames::load(const Vfs &vfs, const std::string &path, const AnimationMetadata &metadata)
{
	S3_TRY_MAP(std::string text, vfs.read_to_string(path), [&](const std::string &e) { return "Cannot read " + path + ": " + e; });
	S3_TRY_MAP(AnimationFrames f, parse(text, metadata), [&](const std::string &e) { return path + ": " + e; });
	return f;
}

static Result<std::vector<SampleWords>> json_samples(JsonValue v)
{
	return json_vec<SampleWords>(v, [](JsonValue x) -> Result<SampleWords> {
		SampleWords w{};
		S3_CHECK(json_u32_array(x, w.data(), 10));
		return w;
	});
}

static Result<ClipFrames> json_clip_frames(JsonValue v)
{
	JsonFields f(v,
	             {"name", "source_offset", "fps_bits", "loop_translation_bits", "loop_rotation_bits", "channel_animation",
	              "channel_weights", "frames"},
	             true);
	if (!f.ok()) return Error{f.error()};
	ClipFrames c;
	S3_TRY(JsonValue x1, f.require("name"));
	S3_TRY(c.name, x1.to_string());
	S3_TRY(JsonValue x2, f.require("source_offset"));
	S3_TRY(c.source_offset, x2.to_u64());
	S3_TRY(JsonValue x3, f.require("fps_bits"));
	S3_TRY(c.fps_bits, x3.to_u32());
	S3_TRY(JsonValue x4, f.require("loop_translation_bits"));
	S3_CHECK(json_u32_array(x4, c.loop_translation_bits.data(), 3));
	S3_TRY(JsonValue x5, f.require("loop_rotation_bits"));
	S3_CHECK(json_u32_array(x5, c.loop_rotation_bits.data(), 4));
	S3_TRY(JsonValue x6, f.require("channel_animation"));
	S3_TRY(c.channel_animation, x6.to_bool());
	S3_TRY(JsonValue x7, f.require("channel_weights"));
	S3_TRY(c.channel_weights, json_u32_vec(x7));
	S3_TRY(JsonValue x8, f.require("frames"));
	S3_TRY(c.frames, json_vec<std::vector<SampleWords>>(x8, json_samples));
	return c;
}

static Result<ReferencePose> json_reference_pose(JsonValue v)
{
	JsonFields f(v, {"name", "source_offset", "samples"}, true);
	if (!f.ok()) return Error{f.error()};
	ReferencePose p;
	S3_TRY(JsonValue x1, f.require("name"));
	S3_TRY(p.name, x1.to_string());
	S3_TRY(JsonValue x2, f.require("source_offset"));
	S3_TRY(p.source_offset, x2.to_u64());
	S3_TRY(JsonValue x3, f.require("samples"));
	S3_TRY(p.samples, json_samples(x3));
	return p;
}

Result<AnimationFrames> AnimationFrames::parse(std::string_view text, const AnimationMetadata &metadata)
{
	S3_TRY(JsonDoc doc, JsonDoc::parse(text));
	JsonFields f(doc.root(),
	             {"version", "source_sha256", "decoder_sha256", "decoder_hardware_verified", "parents", "bone_names", "mirror_indices",
	              "has_trajectory", "clips", "reference_poses"},
	             true);
	if (!f.ok()) return Error{f.error()};
	S3_TRY(JsonValue x1, f.require("version"));
	S3_TRY(uint32_t version, x1.to_u32());
	AnimationFrames out;
	S3_TRY(JsonValue x2, f.require("source_sha256"));
	S3_TRY(out.source_sha256, x2.to_string());
	S3_TRY(JsonValue x3, f.require("decoder_sha256"));
	S3_TRY(out.decoder_sha256, x3.to_string());
	S3_TRY(JsonValue x4, f.require("decoder_hardware_verified"));
	S3_TRY(out.decoder_hardware_verified, x4.to_bool());
	S3_TRY(JsonValue x5, f.require("parents"));
	S3_TRY(out.parents, json_i32_vec(x5));
	S3_TRY(JsonValue x6, f.require("bone_names"));
	S3_TRY(out.bone_names, json_string_vec(x6));
	S3_TRY(JsonValue x7, f.require("mirror_indices"));
	S3_TRY(out.mirror_indices, json_i32_vec(x7));
	S3_TRY(JsonValue x8, f.require("has_trajectory"));
	S3_TRY(out.has_trajectory, x8.to_bool());
	S3_TRY(JsonValue x9, f.require("clips"));
	S3_TRY(std::vector<ClipFrames> clips, json_vec<ClipFrames>(x9, json_clip_frames));
	S3_TRY(JsonValue x10, f.require("reference_poses"));
	S3_TRY(std::vector<ReferencePose> poses, json_vec<ReferencePose>(x10, json_reference_pose));

	if (version != 3 || out.source_sha256 != metadata.source_sha256)
		return Error{"Animation sample format or stock bank identity differs from metadata"};
	bool hex = out.decoder_sha256.size() == 64;
	for (unsigned char c : out.decoder_sha256)
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) hex = false;
	if (!hex) return Error{"Animation sample decoder identity is invalid"};
	size_t bones = out.parents.size();
	if (bones == 0 || bones > 255) return Error{"Animation sample hierarchy has invalid bone count"};
	for (size_t index = 0; index < bones; index++) {
		int32_t parent = out.parents[index];
		if (parent < -1 || parent >= (int32_t)index)
			return Error{"Animation bone" + std::to_string(index) + " has invalid parent" + std::to_string(parent)};
	}
	if (out.bone_names.size() != bones || out.mirror_indices.size() != bones) return Error{"Animation hierarchy names/mirrors are incomplete"};
	std::set<std::string> names;
	for (size_t i = 0; i < bones; i++) {
		int32_t partner = out.mirror_indices[i];
		const std::string &name = out.bone_names[i];
		if (name.empty() || !names.insert(name).second || partner < -1 || partner >= (int32_t)bones ||
		    (partner >= 0 && out.mirror_indices[(size_t)partner] != (int32_t)i))
			return Error{"Animation bone" + std::to_string(i) + " has an invalid name/mirror mapping"};
	}
	for (ClipFrames &clip : clips) {
		S3_TRY(const ClipMetadata *source, metadata.clip(clip.name));
		if (source->source_offset != clip.source_offset || source->fps_bits != clip.fps_bits ||
		    f32_from_bits(source->frames_bits) != (float)clip.frames.size() ||
		    clip.channel_animation != ((source->flags_word & (1u << 29)) != 0))
			return Error{clip.name + ": decoded samples disagree with stock clip header"};
		bool bad = clip.channel_weights.size() != bones;
		for (uint32_t w : clip.channel_weights) bad |= !is_finite(f32_from_bits(w));
		for (uint32_t w : clip.loop_translation_bits) bad |= !is_finite(f32_from_bits(w));
		for (uint32_t w : clip.loop_rotation_bits) bad |= !is_finite(f32_from_bits(w));
		if (bad) return Error{clip.name + ": invalid loop transform or missing channel weights"};
		for (const auto &frame : clip.frames) S3_CHECK(validate_samples(frame, bones));
		std::string name = clip.name;
		if (out.clips_.count(name)) return Error{"Duplicate decoded clip" + name};
		out.clips_.emplace(name, std::move(clip));
	}
	for (ReferencePose &pose : poses) {
		S3_CHECK(validate_samples(pose.samples, bones));
		bool allowed = pose.name == "RIG_TPOSE" || pose.name == "BOARD_BACKWARDS" || pose.name == "BOARD_BACKWARDS_IK";
		if (!allowed || out.reference_poses_.count(pose.source_offset)) return Error{"Invalid or duplicated reference-pose source record"};
		uint64_t key = pose.source_offset;
		out.reference_poses_.emplace(key, std::move(pose));
	}
	return out;
}

Result<const ClipFrames *> AnimationFrames::clip(const std::string &name) const
{
	if (native_) return native_->clip(name);
	auto it = clips_.find(to_ascii_uppercase(name));
	if (it == clips_.end()) return Error{"Required decoded stock clip is absent: " + name};
	return &it->second;
}

Result<const ReferencePose *> AnimationFrames::reference_pose(uint64_t source_offset) const
{
	if (native_) return native_->reference_pose(0, source_offset);
	auto it = reference_poses_.find(source_offset);
	if (it == reference_poses_.end()) return Error{"Required reference-pose record" + hex_prefixed(source_offset) + " is absent"};
	return &it->second;
}

Result<const ReferencePose *> AnimationFrames::named_pose_in(size_t bank, const std::string &name) const
{
	if (native_) return native_->named_pose(bank, name);
	if (bank != 0) return Error{"Converted animation data contains only one bank"};
	for (auto it = reference_poses_.rbegin(); it != reference_poses_.rend(); ++it)
		if (eq_ignore_ascii_case(it->second.name, name)) return &it->second;
	return Error{"Required stock reference pose is absent: " + name};
}

size_t AnimationFrames::clip_count() const { return native_ ? native_->clip_count() : clips_.size(); }

std::vector<std::string> AnimationFrames::clip_names() const
{
	if (native_) return native_->clip_names();
	std::vector<std::string> out;
	for (auto &kv : clips_) out.push_back(kv.first);
	return out;
}

Result<size_t> AnimationFrames::clip_bank(const std::string &name) const
{
	if (native_) return native_->clip_bank(name);
	S3_CHECK(clip(name));
	return (size_t)0;
}

} // namespace s3::data
