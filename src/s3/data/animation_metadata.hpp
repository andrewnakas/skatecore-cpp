// Runtime clip metadata read directly from the stock Andale bank.
// Clock/attribute evaluation belongs to core/game; this loader preserves bits
// and record order and rejects malformed or ambiguous source data.
// (skate-data animation_metadata.rs + animation_metadata/{native,blend_space,selection_space}.rs)
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "abin.hpp"
#include "result.hpp"
#include "vfs.hpp"

namespace s3::data {

/// Bank-relative record offsets are meaningful only with this source identity.
struct BankSource
{
	std::string source_bank;
	std::string source_sha256;
	uint64_t source_bytes = 0;
	bool operator==(const BankSource &o) const
	{
		return source_bank == o.source_bank && source_sha256 == o.source_sha256 && source_bytes == o.source_bytes;
	}
};

struct ClipAttribute
{
	std::string name;
	uint8_t type_id = 0;
	uint32_t begin_bits = 0;
	uint32_t end_bits = 0;
	/// Raw aligned payload, including opaque curve data and padding. Consumers
	/// select the lanes written by Attribute::Init for this type.
	std::vector<uint32_t> payload_words;
	uint64_t source_offset = 0;
};

struct ClipMetadata
{
	std::string name;
	uint64_t source_offset = 0;
	uint32_t fps_bits = 0;
	uint32_t frames_bits = 0;
	uint32_t base_speed_bits = 0;
	uint32_t flags_word = 0;
	std::vector<ClipAttribute> attributes;
};

struct PhaseBlendMetadata
{
	std::string name;
	uint64_t source_offset = 0;
	std::string parameter;
	std::vector<std::string> children;
};

struct SelectorMetadata
{
	std::string name;
	uint64_t source_offset = 0;
	std::string parameter;
	std::string default_; // json "default"
	std::vector<std::string> children;
	std::vector<std::string> values;
};

//! Andale type6: construction82D1B8D0, layout82D22DA0, simplex82D23898.
struct BlendSimplexMetadata
{
	std::vector<size_t> children;
	/// Spatial coordinates only; the serialized matrix also has a final column.
	std::vector<std::vector<uint32_t>> vertex_bits;
	std::vector<std::vector<uint32_t>> normal_bits;
	std::vector<uint32_t> scale_bits;
};

struct BlendSpaceMetadata
{
	std::string name;
	uint64_t source_offset = 0;
	std::vector<std::string> parameters;
	std::vector<std::string> children;
	std::vector<BlendSimplexMetadata> simplexes;
	Status validate(uint64_t bytes) const;
};

//! Type11 layout from original 82D1BEA0, 82D26CC0 and 82D26FF8.
struct SelectionParameterMetadata
{
	std::string name;
	uint32_t mode = 0;
	uint32_t weight_bits = 0;
	uint32_t minimum_bits = 0;
	uint32_t maximum_bits = 0;
};

struct SelectionCandidateMetadata
{
	std::string child;
	std::vector<uint32_t> value_bits;
};

struct SelectionSpaceMetadata
{
	std::string name;
	uint64_t source_offset = 0;
	std::vector<SelectionParameterMetadata> parameters;
	std::vector<SelectionCandidateMetadata> candidates;
	Status validate(uint64_t bytes) const;
};

// TreeMetadata::{Clip, BlendSpace, PhaseBlend, Selector, SelectionSpace}
using TreeMetadata = std::variant<const ClipMetadata *, const BlendSpaceMetadata *, const PhaseBlendMetadata *,
                                  const SelectorMetadata *, const SelectionSpaceMetadata *>;

namespace animation_metadata_detail {
struct UnsupportedTree
{
	std::string name;
	uint64_t source_offset = 0;
	uint32_t type_id = 0;
};
struct File
{
	uint32_t version = 0;
	std::string source_bank;
	std::string source_sha256;
	uint64_t source_bytes = 0;
	std::vector<ClipMetadata> clips;
	std::vector<PhaseBlendMetadata> phase_blends;
	std::vector<BlendSpaceMetadata> blend_spaces;
	std::vector<SelectorMetadata> selectors;
	std::vector<SelectionSpaceMetadata> selection_spaces;
	std::vector<UnsupportedTree> unsupported_trees;
};
} // namespace animation_metadata_detail

class AnimationMetadata
{
public:
	std::string source_bank;
	std::string source_sha256;

	static Result<AnimationMetadata> load(const Vfs &vfs, const std::string &path);
	static Result<AnimationMetadata> parse(std::string_view text);
	/// Read the retained bytes without constructing or evaluating animation
	/// trees. Caller supplies the independently calculated bank SHA-256.
	static Result<AnimationMetadata> from_bank(const abin::Bank &bank, std::string source_bank, std::string source_sha256);

	/// Combine disjoint animation namespaces without comparing offsets from
	/// different files. Same-bank duplicates still obey native last-record wins.
	/// All collision checks precede mutation; the primary rig identity remains.
	Status merge(AnimationMetadata other);
	const std::vector<BankSource> &sources() const { return sources_; }
	const BankSource *source_for(const std::string &name) const;
	/// The serialized names use FastString's canonical uppercase alphabet.
	Result<const ClipMetadata *> clip(const std::string &name) const;
	Result<TreeMetadata> tree(const std::string &name) const;
	size_t clip_count() const;

	static Result<AnimationMetadata> from_file(animation_metadata_detail::File file);

private:
	std::map<std::string, std::vector<ClipMetadata>> clips_;
	std::map<std::string, std::vector<PhaseBlendMetadata>> phase_blends_;
	std::map<std::string, std::vector<BlendSpaceMetadata>> blend_spaces_;
	std::map<std::string, std::vector<SelectorMetadata>> selectors_;
	std::map<std::string, std::vector<SelectionSpaceMetadata>> selection_spaces_;
	std::map<std::string, std::vector<std::pair<uint64_t, uint32_t>>> unsupported_;
	std::vector<BankSource> sources_;
	std::map<std::string, size_t> origins_;
};

// validate_name (FastString canonical names).
Status validate_name(const std::string &name, size_t length);

} // namespace s3::data
