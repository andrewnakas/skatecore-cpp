// Direct stock Andale bank storage and format descriptions.
//
// Source: TU3 DataBase::SetDBContent82D1B340, Clip::Init827B8AB0,
// FetchSys::BatchFetch82D20560/82D20788 and codec identifier82D17A60.
// This module owns bytes and bounds checks; codec arithmetic belongs to core.
// (skate-data abin/{mod,reader,bank}.rs)
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "result.hpp"
#include "vfs.hpp"

namespace s3::data::abin {

using ::s3::data::Error;
using ::s3::data::Result;
using ::s3::data::Status;
using ::s3::data::Unit;

// Rust abin::Error Display: "ABIN at {offset:#x}: {message}". Errors travel formatted.
std::string error_text(size_t offset, const std::string &message);
inline Error error(size_t offset, const std::string &message) { return Error{error_text(offset, message)}; }

struct Range
{
	size_t start = 0;
	size_t end = 0;
	size_t size() const { return end - start; }
	bool operator==(const Range &o) const { return start == o.start && end == o.end; }
};

// Codec::Raw / Codec::Vbr / Codec::Other(word)
struct Codec
{
	enum Kind { Raw, Vbr, Other } kind = Other;
	uint32_t other = 0;
	///82D17A60 reverses the disk word and compares RD\0\0 and VBR\0.
	static Codec from_word(uint32_t word);
	bool operator==(const Codec &o) const { return kind == o.kind && (kind != Other || other == o.other); }
	bool operator!=(const Codec &o) const { return !(*this == o); }
	std::string debug() const; // Rust {:?}: "Raw", "Vbr", "Other(123)"
};

struct RecordHeader
{
	std::string name;
	std::array<uint32_t, 6> name_words{};
	size_t offset = 0;
	size_t size = 0;
	size_t payload_offset = 0;
	uint32_t type_id = 0;
	uint32_t codec_word = 0;
	uint32_t guid = 0;
	Codec codec() const { return Codec::from_word(codec_word); }
	Range range() const { return {offset, offset + size}; }
};

/// Common original AnimationPart header for RAW and VBR. Relative words and
/// absolute byte ranges coexist so consumers never accidentally mix bases.
struct AnimationPart
{
	size_t offset = 0;
	uint32_t header_word = 0;
	uint32_t compressed_size = 0;
	uint32_t compression_header_relative = 0;
	uint32_t compressed_data_relative = 0;
	///Header bits6..11 plus1, verified against all stock hierarchy parts.
	uint16_t channel_count = 0;
	///Packed header bits12..31 give the byte count at compression_header_relative.
	Range compression_header;
	Range compressed_data;
	///Original channel-animation weight words begin at part+16.
	std::optional<Range> channel_weights;
};

struct PartEntry
{
	size_t table_index = 0;
	size_t table_offset = 0;
	uint32_t raw_word = 0;
	uint8_t encoded_id = 0;
	///An authored zero offset remains an empty entry; no table compaction.
	std::optional<AnimationPart> part;
};

struct Clip
{
	uint32_t fps_bits = 0;
	uint32_t frame_count_bits = 0;
	uint32_t base_speed_bits = 0;
	uint32_t flags = 0;
	std::array<uint32_t, 4> loop_translation_words{};
	std::array<uint32_t, 4> loop_rotation_words{};
	size_t attribute_offset = 0;
	uint8_t attribute_count = 0;
	std::vector<PartEntry> parts;
	bool looping() const { return (flags & 0x10000000u) != 0; }
	bool channel_animation() const { return (flags & 0x20000000u) != 0; }
	bool phase_controlled() const { return (flags & 0x40000000u) != 0; }
};

struct Pose
{
	std::vector<PartEntry> parts;
};

struct PhysicsPose
{
	uint32_t bone_count = 0;
	///24 authored112-byte PhysicsParamBoneData records in the stock banks.
	Range records;
};

struct HierarchyPart
{
	std::string name;
	uint32_t bone_count = 0;
	int32_t sqt_offset = 0;
	///Retained original words; no runtime part-id rewriting is performed.
	uint16_t flags = 0;
	uint16_t encoded_id = 0;
	bool operator==(const HierarchyPart &o) const
	{
		return name == o.name && bone_count == o.bone_count && sqt_offset == o.sqt_offset && flags == o.flags && encoded_id == o.encoded_id;
	}
};

struct Hierarchy
{
	size_t offset = 0;
	uint16_t bone_count = 0;
	bool has_trajectory = false;
	std::vector<int32_t> parents;
	std::vector<int32_t> mirrors;
	std::vector<std::string> bone_names;
	std::vector<HierarchyPart> parts;
	bool compatible_with(const Hierarchy &other) const;
};

struct Opaque
{
};

// RecordData::{Clip, Pose, Hierarchy, PhysicsPose, Opaque}
using RecordData = std::variant<Clip, Pose, Hierarchy, PhysicsPose, Opaque>;

struct Record
{
	RecordHeader header;
	RecordData data;
};

//! Checked big-endian reads with absolute offsets retained through subrecords.
class Reader
{
public:
	Reader() = default;
	explicit Reader(const uint8_t *bytes, size_t size) : bytes_(bytes), begin_(0), end_(size) {}
	Result<Reader> bounded(Range range) const;
	Result<const uint8_t *> take(size_t offset, size_t size) const;
	Result<uint8_t> u8(size_t offset) const;
	Result<uint16_t> u16(size_t offset) const;
	Result<uint32_t> u32(size_t offset) const;
	Result<int32_t> i32(size_t offset) const;
	template <size_t N>
	Result<std::array<uint32_t, N>> words(size_t offset) const
	{
		S3_TRY(const uint8_t *raw, take(offset, N * 4));
		std::array<uint32_t, N> out{};
		for (size_t i = 0; i < N; i++)
			out[i] = (uint32_t)raw[4 * i] << 24 | (uint32_t)raw[4 * i + 1] << 16 | (uint32_t)raw[4 * i + 2] << 8 | raw[4 * i + 3];
		return out;
	}
	///Five words encode FastString30, six encode FastString36. Names stay
	///uppercase as authored; padding remains word-local, matching stock names.
	Result<std::string> name(size_t offset, size_t words) const;
	Result<Range> relative(size_t base, uint32_t offset, size_t size) const;

private:
	const uint8_t *bytes_ = nullptr;
	size_t begin_ = 0;
	size_t end_ = 0;
};

class Bank
{
public:
	size_t wrapper_bytes = 0;
	Range trailer;

	// `path` is a VFS path. The bank shares the VFS bytes (no copy).
	static Result<Bank> load(const Vfs &vfs, const std::string &path);
	static Result<Bank> parse(SharedBytes bytes);
	static Result<Bank> parse(std::vector<uint8_t> bytes) { return parse(SharedBytes::from_vec(std::move(bytes))); }

	const uint8_t *bytes() const { return bytes_.data; }
	size_t size() const { return bytes_.size; }
	const SharedBytes &shared_bytes() const { return bytes_; }
	const std::vector<Record> &records() const { return records_; }
	const Record *record_at(size_t offset) const;
	const Record *animation(const std::string &name) const;
	// (header, clip) or nulls.
	std::pair<const RecordHeader *, const Clip *> clip(const std::string &name) const;
	std::pair<const RecordHeader *, const Pose *> pose(const std::string &name) const;
	const Hierarchy *hierarchy() const;
	///The stock OnBoard/OffBoard VBR banks carry all eight parts in hierarchy
	///order. Their encoded ids are all zero; source82D20608/82D20924 advances
	///its equal-id cursor one entry at a time, preserving this ordinal mapping.
	///Call before using positional part association; partial banks may require
	///the original named/id association and are not silently treated as full.
	Status validate_positional_parts() const;

private:
	SharedBytes bytes_;
	std::vector<Record> records_;
	std::map<size_t, size_t> offsets_;
	std::map<std::string, size_t> animations_;
	std::map<std::string, size_t> clips_;
	std::map<std::string, size_t> poses_;
	std::optional<size_t> hierarchy_;
};

} // namespace s3::data::abin
