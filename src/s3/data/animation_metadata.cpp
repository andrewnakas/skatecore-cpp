// Runtime clip metadata (animation_metadata.rs, native.rs, blend_space.rs, selection_space.rs).
#include "animation_metadata.hpp"

#include <algorithm>
#include <limits>

#include "json.hpp"

namespace s3::data {

using animation_metadata_detail::File;
using animation_metadata_detail::UnsupportedTree;

Status validate_name(const std::string &name, size_t length)
{
	bool ok = !name.empty() && name.size() <= length;
	for (unsigned char c : name)
		if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) ok = false;
	if (!ok) return Error{"Invalid canonical FastString name " + debug_str(name)};
	return Unit{};
}

// ---- blend_space.rs ----

Status BlendSpaceMetadata::validate(uint64_t bytes) const
{
	S3_CHECK(validate_name(name, 36));
	size_t d = parameters.size();
	if (source_offset >= bytes || d == 0 || d > 4 || children.size() < d + 1 || simplexes.empty())
		return Error{name + ": invalid BlendSpace dimensions/offset"};
	for (const std::string &n : parameters) S3_CHECK(validate_name(n, 30));
	for (const std::string &n : children) S3_CHECK(validate_name(n, 36));
	for (const BlendSimplexMetadata &s : simplexes) {
		bool bad = s.children.size() != d + 1 || s.scale_bits.size() != d + 1 || s.vertex_bits.size() != d + 1 ||
		           s.normal_bits.size() != d + 1;
		for (size_t i : s.children) bad |= i >= children.size();
		for (auto *list : {&s.vertex_bits, &s.normal_bits})
			for (const auto &v : *list) bad |= v.size() != d;
		for (auto *list : {&s.vertex_bits, &s.normal_bits})
			for (const auto &v : *list)
				for (uint32_t w : v) bad |= !is_finite(f32_from_bits(w));
		for (uint32_t w : s.scale_bits) bad |= !is_finite(f32_from_bits(w));
		if (bad) return Error{name + ": invalid BlendSpace simplex"};
	}
	return Unit{};
}

namespace {

using abin::Range;
using abin::Reader;
using abin::RecordHeader;

Result<BlendSpaceMetadata> read_blend_space(const Reader &r, const RecordHeader &h)
{
	size_t p = h.payload_offset;
	S3_TRY(uint32_t count32, r.u32(p));
	size_t count = count32;
	if (count > std::numeric_limits<size_t>::max() / 24) return abin::error(p, "BlendSpace count overflow");
	size_t names_size = count * 24;
	S3_TRY(uint32_t names_rel, r.u32(p + 4));
	S3_TRY(Range names_r, r.relative(p, names_rel, names_size));
	size_t names = names_r.start;
	S3_TRY(uint32_t q_rel, r.u32(p + 12));
	S3_TRY(Range q_r, r.relative(p, q_rel, 44));
	size_t q = q_r.start;
	S3_TRY(uint32_t space_size, r.u32(q));
	S3_TRY(Range space_r, r.relative(q, 0, space_size));
	S3_TRY(Reader space, r.bounded(space_r));
	S3_TRY(uint32_t d32, space.u32(q + 28));
	size_t d = d32;
	if (d == 0 || d > 4) return abin::error(q + 28, "invalid BlendSpace dimension");
	S3_TRY(uint32_t limit, r.u32(p + 16));
	if (d > (size_t)limit) return abin::error(q + 28, "invalid BlendSpace dimension");
	S3_TRY(uint32_t params_rel, space.u32(q + 32));
	S3_TRY(Range params_r, space.relative(q, params_rel, d * 20));
	size_t params = params_r.start;
	S3_TRY(uint32_t simplex_count32, space.u32(q + 36));
	size_t simplex_count = simplex_count32;
	// Every record must at least contain its header; bound the count before allocating.
	S3_TRY(uint32_t simplex_rel, space.u32(q + 40));
	if (simplex_count > std::numeric_limits<size_t>::max() / 20) return abin::error(q, "BlendSpace simplex count overflow");
	S3_CHECK(space.relative(q, simplex_rel, simplex_count * 20));
	S3_TRY(Range first, space.relative(q, simplex_rel, 20));
	size_t at = first.start;
	std::vector<BlendSimplexMetadata> simplexes;
	for (size_t k = 0; k < simplex_count; k++) {
		S3_TRY(uint32_t size32, space.u32(at));
		size_t size = size32;
		S3_TRY(Range sr, space.relative(at, 0, size));
		S3_TRY(Reader s, space.bounded(sr));
		S3_TRY(uint32_t dim, s.u32(at + 16));
		if ((size_t)dim != d + 1) return abin::error(at + 16, "BlendSpace matrix dimension mismatch");
		S3_CHECK(s.take(at + 20, (d + 1) * (d + 1) * 4));
		S3_TRY(uint32_t ind_rel, s.u32(at + 4));
		S3_TRY(Range ind_r, s.relative(at, ind_rel, (d + 1) * 4));
		size_t indices = ind_r.start;
		S3_TRY(uint32_t sc_rel, s.u32(at + 8));
		S3_TRY(Range sc_r, s.relative(at, sc_rel, (d + 1) * 4));
		size_t scales = sc_r.start;
		S3_TRY(uint32_t nr_rel, s.u32(at + 12));
		S3_TRY(Range nr_r, s.relative(at, nr_rel, (d + 1) * (d + 1) * 4));
		size_t normals = nr_r.start;
		BlendSimplexMetadata simplex;
		for (size_t i = 0; i <= d; i++) {
			size_t normal = normals + i * (d + 1) * 4;
			S3_TRY(uint32_t nd, s.u32(normal));
			if ((size_t)nd != d) return abin::error(normal, "BlendSpace normal dimension mismatch");
			S3_TRY(uint32_t child, s.u32(indices + i * 4));
			simplex.children.push_back(child);
			S3_TRY(uint32_t scale, s.u32(scales + i * 4));
			simplex.scale_bits.push_back(scale);
			std::vector<uint32_t> vertex;
			for (size_t j = 0; j < d; j++) {
				S3_TRY(uint32_t w, s.u32(at + 20 + (i * (d + 1) + j) * 4));
				vertex.push_back(w);
			}
			simplex.vertex_bits.push_back(std::move(vertex));
			std::vector<uint32_t> nv;
			for (size_t j = 0; j < d; j++) {
				S3_TRY(uint32_t w, s.u32(normal + 4 + j * 4));
				nv.push_back(w);
			}
			simplex.normal_bits.push_back(std::move(nv));
		}
		simplexes.push_back(std::move(simplex));
		at += size;
	}
	BlendSpaceMetadata out;
	out.name = h.name;
	out.source_offset = h.offset;
	for (size_t i = 0; i < d; i++) {
		S3_TRY(std::string n, space.name(params + i * 20, 5));
		out.parameters.push_back(std::move(n));
	}
	for (size_t i = 0; i < count; i++) {
		S3_TRY(std::string n, r.name(names + i * 24, 6));
		out.children.push_back(std::move(n));
	}
	out.simplexes = std::move(simplexes);
	return out;
}

// ---- selection_space.rs ----

Result<SelectionSpaceMetadata> read_selection_space(const Reader &r, const RecordHeader &h)
{
	size_t p = h.payload_offset;
	S3_TRY(uint32_t count32, r.u32(p));
	size_t count = count32;
	if (count > 10) return abin::error(p, "selection parameter array exceeds native capacity");
	S3_TRY(uint32_t par_rel, r.u32(p + 4));
	S3_TRY(Range par_r, r.relative(p, par_rel, count * 40));
	size_t parameters_at = par_r.start;
	S3_TRY(uint32_t cand_count32, r.u32(p + 8));
	size_t candidates_count = cand_count32;
	S3_TRY(uint32_t cand_rel, r.u32(p + 12));
	S3_TRY(Range cand_r, r.relative(p, cand_rel, 4));
	size_t candidates_at = cand_r.start;
	S3_TRY(uint32_t stride32, r.u32(candidates_at));
	size_t stride = stride32;
	if (candidates_count != 0 && stride > std::numeric_limits<size_t>::max() / candidates_count)
		return abin::error(p, "selection candidate size overflow");
	S3_CHECK(r.take(candidates_at, stride * candidates_count));
	if (stride < 32) return abin::error(candidates_at, "selection candidate header too small");
	SelectionSpaceMetadata out;
	for (size_t i = 0; i < count; i++) {
		size_t at = parameters_at + i * 40;
		SelectionParameterMetadata m;
		S3_TRY(m.name, r.name(at, 6));
		S3_TRY(m.mode, r.u32(at + 24));
		S3_TRY(m.weight_bits, r.u32(at + 28));
		S3_TRY(m.minimum_bits, r.u32(at + 32));
		S3_TRY(m.maximum_bits, r.u32(at + 36));
		out.parameters.push_back(std::move(m));
	}
	for (size_t i = 0; i < candidates_count; i++) {
		size_t at = candidates_at + i * stride;
		S3_TRY(Reader child_reader, r.bounded(Range{at, at + stride}));
		S3_TRY(uint32_t values_rel, child_reader.u32(at + 28));
		S3_TRY(Range values_r, child_reader.relative(at, values_rel, count * 4));
		size_t values = values_r.start;
		SelectionCandidateMetadata c;
		S3_TRY(c.child, child_reader.name(at + 4, 6));
		for (size_t j = 0; j < count; j++) {
			S3_TRY(uint32_t v, child_reader.u32(values + j * 4));
			c.value_bits.push_back(v);
		}
		out.candidates.push_back(std::move(c));
	}
	out.name = h.name;
	out.source_offset = h.offset;
	return out;
}

// ---- native.rs ----

Result<ClipMetadata> read_clip(const Reader &r, const RecordHeader &h, const abin::Clip &clip)
{
	size_t at = clip.attribute_offset;
	std::vector<ClipAttribute> attributes;
	attributes.reserve(clip.attribute_count);
	for (unsigned k = 0; k < clip.attribute_count; k++) {
		// Original GetAttribute82D25CA0 advances by the record's first word.
		S3_TRY(uint32_t size32, r.u32(at));
		size_t size = size32;
		if (size < 48) return abin::error(at, "attribute record smaller than header");
		if (size > std::numeric_limits<size_t>::max() - at) return abin::error(at, "attribute size overflow");
		size_t end = at + size;
		S3_TRY(Reader a, r.bounded(Range{at, end}));
		if (at > std::numeric_limits<size_t>::max() - 51) return abin::error(at, "attribute alignment overflow");
		size_t data = (at + 51) & ~(size_t)15;
		if (end < data) return abin::error(at, "attribute payload outside record");
		S3_TRY(const uint8_t *payload, a.take(data, end - data));
		size_t payload_len = end - data;
		if (payload_len % 4 != 0) return abin::error(at, "unaligned attribute payload");
		ClipAttribute attr;
		S3_TRY(attr.name, a.name(at + 12, 5));
		S3_TRY(attr.type_id, a.u8(at + 32));
		S3_TRY(attr.begin_bits, a.u32(at + 4));
		S3_TRY(attr.end_bits, a.u32(at + 8));
		attr.payload_words.reserve(payload_len / 4);
		for (size_t i = 0; i < payload_len; i += 4)
			attr.payload_words.push_back((uint32_t)payload[i] << 24 | (uint32_t)payload[i + 1] << 16 | (uint32_t)payload[i + 2] << 8 | payload[i + 3]);
		attr.source_offset = at;
		attributes.push_back(std::move(attr));
		at = end;
	}
	ClipMetadata out;
	out.name = h.name;
	out.source_offset = h.offset;
	out.fps_bits = clip.fps_bits;
	out.frames_bits = clip.frame_count_bits;
	out.base_speed_bits = clip.base_speed_bits;
	out.flags_word = clip.flags;
	out.attributes = std::move(attributes);
	return out;
}

Result<PhaseBlendMetadata> read_phase_blend(const Reader &r, const RecordHeader &h)
{
	// Type7 construction82D1B5B8: count+8, parameter+16, child names+36.
	size_t p = h.payload_offset;
	S3_TRY(uint32_t count32, r.u32(p + 8));
	size_t count = count32;
	if (count > std::numeric_limits<size_t>::max() / 24) return abin::error(p, "PhaseBlend child count overflow");
	S3_CHECK(r.take(p + 36, count * 24));
	PhaseBlendMetadata out;
	for (size_t i = 0; i < count; i++) {
		S3_TRY(std::string n, r.name(p + 36 + 24 * i, 6));
		out.children.push_back(std::move(n));
	}
	out.name = h.name;
	out.source_offset = h.offset;
	S3_TRY(out.parameter, r.name(p + 16, 5));
	return out;
}

Result<SelectorMetadata> read_selector(const Reader &r, const RecordHeader &h)
{
	// Type8 construction82D1B5B8 keeps the first name as authored default.
	// Child/value order matters: runtime chooses the first matching value.
	size_t p = h.payload_offset;
	S3_TRY(uint32_t count32, r.u32(p + 28));
	size_t count = count32;
	size_t names_size = std::max<size_t>(count, 1) * 24;
	size_t keys_size = count * 20;
	S3_TRY(uint32_t names_rel, r.u32(p + 20));
	S3_TRY(Range names_r, r.relative(p, names_rel, names_size));
	size_t names = names_r.start;
	S3_TRY(uint32_t keys_rel, r.u32(p + 24));
	S3_TRY(Range keys_r, r.relative(p, keys_rel, keys_size));
	size_t keys = keys_r.start;
	SelectorMetadata out;
	out.name = h.name;
	out.source_offset = h.offset;
	S3_TRY(out.parameter, r.name(p, 5));
	S3_TRY(out.default_, r.name(names, 6));
	for (size_t i = 0; i < count; i++) {
		S3_TRY(std::string n, r.name(names + 24 * i, 6));
		out.children.push_back(std::move(n));
	}
	for (size_t i = 0; i < count; i++) {
		S3_TRY(std::string n, r.name(keys + 20 * i, 5));
		out.values.push_back(std::move(n));
	}
	return out;
}

// ---- JSON (serde) ----

Result<ClipAttribute> json_attribute(JsonValue v)
{
	JsonFields f(v, {"name", "type_id", "begin_bits", "end_bits", "payload_words", "source_offset"}, true);
	if (!f.ok()) return Error{f.error()};
	ClipAttribute a;
	S3_TRY(JsonValue x1, f.require("name"));
	S3_TRY(a.name, x1.to_string());
	S3_TRY(JsonValue x2, f.require("type_id"));
	S3_TRY(a.type_id, x2.to_u8());
	S3_TRY(JsonValue x3, f.require("begin_bits"));
	S3_TRY(a.begin_bits, x3.to_u32());
	S3_TRY(JsonValue x4, f.require("end_bits"));
	S3_TRY(a.end_bits, x4.to_u32());
	S3_TRY(JsonValue x5, f.require("payload_words"));
	S3_TRY(a.payload_words, json_u32_vec(x5));
	S3_TRY(JsonValue x6, f.require("source_offset"));
	S3_TRY(a.source_offset, x6.to_u64());
	return a;
}

Result<ClipMetadata> json_clip(JsonValue v)
{
	JsonFields f(v, {"name", "source_offset", "fps_bits", "frames_bits", "base_speed_bits", "flags_word", "attributes"}, true);
	if (!f.ok()) return Error{f.error()};
	ClipMetadata c;
	S3_TRY(JsonValue x1, f.require("name"));
	S3_TRY(c.name, x1.to_string());
	S3_TRY(JsonValue x2, f.require("source_offset"));
	S3_TRY(c.source_offset, x2.to_u64());
	S3_TRY(JsonValue x3, f.require("fps_bits"));
	S3_TRY(c.fps_bits, x3.to_u32());
	S3_TRY(JsonValue x4, f.require("frames_bits"));
	S3_TRY(c.frames_bits, x4.to_u32());
	S3_TRY(JsonValue x5, f.require("base_speed_bits"));
	S3_TRY(c.base_speed_bits, x5.to_u32());
	S3_TRY(JsonValue x6, f.require("flags_word"));
	S3_TRY(c.flags_word, x6.to_u32());
	S3_TRY(JsonValue x7, f.require("attributes"));
	S3_TRY(c.attributes, json_vec<ClipAttribute>(x7, json_attribute));
	return c;
}

Result<PhaseBlendMetadata> json_phase_blend(JsonValue v)
{
	JsonFields f(v, {"name", "source_offset", "parameter", "children"}, true);
	if (!f.ok()) return Error{f.error()};
	PhaseBlendMetadata t;
	S3_TRY(JsonValue x1, f.require("name"));
	S3_TRY(t.name, x1.to_string());
	S3_TRY(JsonValue x2, f.require("source_offset"));
	S3_TRY(t.source_offset, x2.to_u64());
	S3_TRY(JsonValue x3, f.require("parameter"));
	S3_TRY(t.parameter, x3.to_string());
	S3_TRY(JsonValue x4, f.require("children"));
	S3_TRY(t.children, json_string_vec(x4));
	return t;
}

Result<std::vector<std::vector<uint32_t>>> json_u32_matrix(JsonValue v)
{
	return json_vec<std::vector<uint32_t>>(v, json_u32_vec);
}

Result<BlendSimplexMetadata> json_simplex(JsonValue v)
{
	JsonFields f(v, {"children", "vertex_bits", "normal_bits", "scale_bits"}, true);
	if (!f.ok()) return Error{f.error()};
	BlendSimplexMetadata s;
	S3_TRY(JsonValue x1, f.require("children"));
	S3_TRY(s.children, json_vec<size_t>(x1, [](JsonValue x) -> Result<size_t> {
		       S3_TRY(uint64_t u, x.to_u64());
		       return (size_t)u;
	       }));
	S3_TRY(JsonValue x2, f.require("vertex_bits"));
	S3_TRY(s.vertex_bits, json_u32_matrix(x2));
	S3_TRY(JsonValue x3, f.require("normal_bits"));
	S3_TRY(s.normal_bits, json_u32_matrix(x3));
	S3_TRY(JsonValue x4, f.require("scale_bits"));
	S3_TRY(s.scale_bits, json_u32_vec(x4));
	return s;
}

Result<BlendSpaceMetadata> json_blend_space(JsonValue v)
{
	JsonFields f(v, {"name", "source_offset", "parameters", "children", "simplexes"}, true);
	if (!f.ok()) return Error{f.error()};
	BlendSpaceMetadata t;
	S3_TRY(JsonValue x1, f.require("name"));
	S3_TRY(t.name, x1.to_string());
	S3_TRY(JsonValue x2, f.require("source_offset"));
	S3_TRY(t.source_offset, x2.to_u64());
	S3_TRY(JsonValue x3, f.require("parameters"));
	S3_TRY(t.parameters, json_string_vec(x3));
	S3_TRY(JsonValue x4, f.require("children"));
	S3_TRY(t.children, json_string_vec(x4));
	S3_TRY(JsonValue x5, f.require("simplexes"));
	S3_TRY(t.simplexes, json_vec<BlendSimplexMetadata>(x5, json_simplex));
	return t;
}

Result<SelectorMetadata> json_selector(JsonValue v)
{
	JsonFields f(v, {"name", "source_offset", "parameter", "default", "children", "values"}, true);
	if (!f.ok()) return Error{f.error()};
	SelectorMetadata t;
	S3_TRY(JsonValue x1, f.require("name"));
	S3_TRY(t.name, x1.to_string());
	S3_TRY(JsonValue x2, f.require("source_offset"));
	S3_TRY(t.source_offset, x2.to_u64());
	S3_TRY(JsonValue x3, f.require("parameter"));
	S3_TRY(t.parameter, x3.to_string());
	S3_TRY(JsonValue x4, f.require("default"));
	S3_TRY(t.default_, x4.to_string());
	S3_TRY(JsonValue x5, f.require("children"));
	S3_TRY(t.children, json_string_vec(x5));
	S3_TRY(JsonValue x6, f.require("values"));
	S3_TRY(t.values, json_string_vec(x6));
	return t;
}

Result<SelectionParameterMetadata> json_selection_parameter(JsonValue v)
{
	JsonFields f(v, {"name", "mode", "weight_bits", "minimum_bits", "maximum_bits"}, true);
	if (!f.ok()) return Error{f.error()};
	SelectionParameterMetadata p;
	S3_TRY(JsonValue x1, f.require("name"));
	S3_TRY(p.name, x1.to_string());
	S3_TRY(JsonValue x2, f.require("mode"));
	S3_TRY(p.mode, x2.to_u32());
	S3_TRY(JsonValue x3, f.require("weight_bits"));
	S3_TRY(p.weight_bits, x3.to_u32());
	S3_TRY(JsonValue x4, f.require("minimum_bits"));
	S3_TRY(p.minimum_bits, x4.to_u32());
	S3_TRY(JsonValue x5, f.require("maximum_bits"));
	S3_TRY(p.maximum_bits, x5.to_u32());
	return p;
}

Result<SelectionCandidateMetadata> json_selection_candidate(JsonValue v)
{
	JsonFields f(v, {"child", "value_bits"}, true);
	if (!f.ok()) return Error{f.error()};
	SelectionCandidateMetadata c;
	S3_TRY(JsonValue x1, f.require("child"));
	S3_TRY(c.child, x1.to_string());
	S3_TRY(JsonValue x2, f.require("value_bits"));
	S3_TRY(c.value_bits, json_u32_vec(x2));
	return c;
}

Result<SelectionSpaceMetadata> json_selection_space(JsonValue v)
{
	JsonFields f(v, {"name", "source_offset", "parameters", "candidates"}, true);
	if (!f.ok()) return Error{f.error()};
	SelectionSpaceMetadata t;
	S3_TRY(JsonValue x1, f.require("name"));
	S3_TRY(t.name, x1.to_string());
	S3_TRY(JsonValue x2, f.require("source_offset"));
	S3_TRY(t.source_offset, x2.to_u64());
	S3_TRY(JsonValue x3, f.require("parameters"));
	S3_TRY(t.parameters, json_vec<SelectionParameterMetadata>(x3, json_selection_parameter));
	S3_TRY(JsonValue x4, f.require("candidates"));
	S3_TRY(t.candidates, json_vec<SelectionCandidateMetadata>(x4, json_selection_candidate));
	return t;
}

Result<UnsupportedTree> json_unsupported(JsonValue v)
{
	JsonFields f(v, {"name", "source_offset", "type_id"}, true);
	if (!f.ok()) return Error{f.error()};
	UnsupportedTree t;
	S3_TRY(JsonValue x1, f.require("name"));
	S3_TRY(t.name, x1.to_string());
	S3_TRY(JsonValue x2, f.require("source_offset"));
	S3_TRY(t.source_offset, x2.to_u64());
	S3_TRY(JsonValue x3, f.require("type_id"));
	S3_TRY(t.type_id, x3.to_u32());
	return t;
}

template <class T, class F>
Status optional_vec(const JsonFields &f, const char *name, std::vector<T> &out, F parse)
{
	JsonValue v = f.get(name);
	if (!v.valid()) return Unit{}; // #[serde(default)]
	S3_TRY(out, json_vec<T>(v, parse));
	return Unit{};
}

Result<File> json_file(std::string_view text)
{
	S3_TRY(JsonDoc doc, JsonDoc::parse(text));
	JsonFields f(doc.root(),
	             {"version", "source_bank", "source_sha256", "source_bytes", "clips", "phase_blends", "blend_spaces", "selectors",
	              "selection_spaces", "unsupported_trees"},
	             true);
	if (!f.ok()) return Error{f.error()};
	File file;
	S3_TRY(JsonValue x1, f.require("version"));
	S3_TRY(file.version, x1.to_u32());
	S3_TRY(JsonValue x2, f.require("source_bank"));
	S3_TRY(file.source_bank, x2.to_string());
	S3_TRY(JsonValue x3, f.require("source_sha256"));
	S3_TRY(file.source_sha256, x3.to_string());
	S3_TRY(JsonValue x4, f.require("source_bytes"));
	S3_TRY(file.source_bytes, x4.to_u64());
	S3_TRY(JsonValue x5, f.require("clips"));
	S3_TRY(file.clips, json_vec<ClipMetadata>(x5, json_clip));
	S3_CHECK(optional_vec(f, "phase_blends", file.phase_blends, json_phase_blend));
	S3_CHECK(optional_vec(f, "blend_spaces", file.blend_spaces, json_blend_space));
	S3_CHECK(optional_vec(f, "selectors", file.selectors, json_selector));
	S3_CHECK(optional_vec(f, "selection_spaces", file.selection_spaces, json_selection_space));
	S3_TRY(JsonValue x6, f.require("unsupported_trees"));
	S3_TRY(file.unsupported_trees, json_vec<UnsupportedTree>(x6, json_unsupported));
	return file;
}

} // namespace

Status SelectionSpaceMetadata::validate(uint64_t bytes) const
{
	S3_CHECK(validate_name(name, 36));
	if (source_offset >= bytes || parameters.size() > 10 || candidates.empty())
		return Error{name + ": invalid selection space dimensions"};
	for (const SelectionParameterMetadata &p : parameters) {
		S3_CHECK(validate_name(p.name, 30));
		for (uint32_t v : {p.weight_bits, p.minimum_bits, p.maximum_bits})
			if (!is_finite(f32_from_bits(v))) return Error{name + ": nonfinite selection metric"};
	}
	for (const SelectionCandidateMetadata &c : candidates) {
		S3_CHECK(validate_name(c.child, 36));
		bool bad = c.value_bits.size() != parameters.size();
		for (uint32_t v : c.value_bits) bad |= !is_finite(f32_from_bits(v));
		if (bad) return Error{name + ": invalid selection candidate"};
	}
	return Unit{};
}

// ---- AnimationMetadata ----

Result<AnimationMetadata> AnimationMetadata::load(const Vfs &vfs, const std::string &path)
{
	S3_TRY_MAP(std::string text, vfs.read_to_string(path),
	           [&](const std::string &e) { return "Cannot read animation metadata " + path + ": " + e; });
	S3_TRY_MAP(AnimationMetadata m, parse(text), [&](const std::string &e) { return path + ": " + e; });
	return m;
}

Result<AnimationMetadata> AnimationMetadata::parse(std::string_view text)
{
	S3_TRY(File file, json_file(text));
	return from_file(std::move(file));
}

Result<AnimationMetadata> AnimationMetadata::from_file(File file)
{
	if (file.version != 1) return Error{"Unsupported animation metadata version " + std::to_string(file.version)};
	bool hex = true;
	for (unsigned char c : file.source_sha256)
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) hex = false;
	if (file.source_bank.empty() || file.source_sha256.size() != 64 || !hex || file.source_bytes < 48)
		return Error{"Invalid animation bank source identity"};
	AnimationMetadata m;
	for (ClipMetadata &clip : file.clips) {
		S3_CHECK(validate_name(clip.name, 36));
		float fps = f32_from_bits(clip.fps_bits);
		float frames = f32_from_bits(clip.frames_bits);
		float base = f32_from_bits(clip.base_speed_bits);
		if (!is_finite(fps) || fps <= 0.0f || !is_finite(frames) || frames < 1.0f || !is_finite(base) || base <= 0.0f ||
		    clip.source_offset >= file.source_bytes)
			return Error{clip.name + ": invalid clip header"};
		uint64_t previous = clip.source_offset;
		for (const ClipAttribute &attribute : clip.attributes) {
			S3_CHECK(validate_name(attribute.name, 30));
			if (!is_finite(f32_from_bits(attribute.begin_bits)) || !is_finite(f32_from_bits(attribute.end_bits)) ||
			    attribute.source_offset <= previous || attribute.source_offset >= file.source_bytes)
				return Error{clip.name + ": invalid attribute record/order"};
			size_t minimum = attribute.type_id == 0 ? 1 : attribute.type_id == 1 ? 4 : attribute.type_id == 3 ? 6 : 0;
			if (attribute.payload_words.size() < minimum) return Error{clip.name + ": truncated " + attribute.name + " payload"};
			previous = attribute.source_offset;
		}
		std::string name = clip.name;
		m.clips_[name].push_back(std::move(clip));
	}
	for (PhaseBlendMetadata &tree : file.phase_blends) {
		S3_CHECK(validate_name(tree.name, 36));
		S3_CHECK(validate_name(tree.parameter, 30));
		if (tree.children.size() < 2) return Error{tree.name + ": PhaseBlend needs two children"};
		if (tree.source_offset >= file.source_bytes) return Error{tree.name + ": tree offset outside bank"};
		for (const std::string &child : tree.children) S3_CHECK(validate_name(child, 36));
		std::string name = tree.name;
		m.phase_blends_[name].push_back(std::move(tree));
	}
	for (BlendSpaceMetadata &tree : file.blend_spaces) {
		S3_CHECK(tree.validate(file.source_bytes));
		std::string name = tree.name;
		m.blend_spaces_[name].push_back(std::move(tree));
	}
	for (SelectorMetadata &tree : file.selectors) {
		S3_CHECK(validate_name(tree.name, 36));
		S3_CHECK(validate_name(tree.parameter, 30));
		S3_CHECK(validate_name(tree.default_, 36));
		if (tree.children.size() != tree.values.size()) return Error{tree.name + ": selector key/child count mismatch"};
		if (tree.source_offset >= file.source_bytes) return Error{tree.name + ": tree offset outside bank"};
		for (const std::string &child : tree.children) S3_CHECK(validate_name(child, 36));
		for (const std::string &value : tree.values) S3_CHECK(validate_name(value, 30));
		std::string name = tree.name;
		m.selectors_[name].push_back(std::move(tree));
	}
	for (SelectionSpaceMetadata &tree : file.selection_spaces) {
		S3_CHECK(tree.validate(file.source_bytes));
		std::string name = tree.name;
		m.selection_spaces_[name].push_back(std::move(tree));
	}
	for (UnsupportedTree &tree : file.unsupported_trees) {
		S3_CHECK(validate_name(tree.name, 36));
		if (tree.source_offset >= file.source_bytes) return Error{tree.name + ": tree offset outside bank"};
		m.unsupported_[tree.name].push_back({tree.source_offset, tree.type_id});
	}
	for (auto &kv : m.clips_) m.origins_[kv.first] = 0;
	for (auto &kv : m.phase_blends_) m.origins_[kv.first] = 0;
	for (auto &kv : m.blend_spaces_) m.origins_[kv.first] = 0;
	for (auto &kv : m.selectors_) m.origins_[kv.first] = 0;
	for (auto &kv : m.selection_spaces_) m.origins_[kv.first] = 0;
	for (auto &kv : m.unsupported_) m.origins_[kv.first] = 0;
	m.sources_.push_back(BankSource{file.source_bank, file.source_sha256, file.source_bytes});
	m.source_bank = std::move(file.source_bank);
	m.source_sha256 = std::move(file.source_sha256);
	return m;
}

Result<AnimationMetadata> AnimationMetadata::from_bank(const abin::Bank &bank, std::string source_bank, std::string source_sha256)
{
	File file;
	file.version = 1;
	file.source_bank = std::move(source_bank);
	file.source_sha256 = std::move(source_sha256);
	file.source_bytes = bank.size();
	Reader reader(bank.bytes(), bank.size());
	for (const abin::Record &record : bank.records()) {
		const RecordHeader &h = record.header;
		S3_TRY(Reader r, reader.bounded(h.range()));
		if (auto *clip = std::get_if<abin::Clip>(&record.data)) {
			S3_TRY(ClipMetadata c, read_clip(r, h, *clip));
			file.clips.push_back(std::move(c));
		} else if (std::holds_alternative<abin::Opaque>(record.data)) {
			switch (h.type_id) {
			case 6: {
				S3_TRY(BlendSpaceMetadata t, read_blend_space(r, h));
				file.blend_spaces.push_back(std::move(t));
				break;
			}
			case 7: {
				S3_TRY(PhaseBlendMetadata t, read_phase_blend(r, h));
				file.phase_blends.push_back(std::move(t));
				break;
			}
			case 8: {
				S3_TRY(SelectorMetadata t, read_selector(r, h));
				file.selectors.push_back(std::move(t));
				break;
			}
			case 11: {
				S3_TRY(SelectionSpaceMetadata t, read_selection_space(r, h));
				file.selection_spaces.push_back(std::move(t));
				break;
			}
			default: file.unsupported_trees.push_back(UnsupportedTree{h.name, (uint64_t)h.offset, h.type_id}); break;
			}
		}
	}
	return from_file(std::move(file));
}

Status AnimationMetadata::merge(AnimationMetadata other)
{
	for (auto &kv : other.origins_)
		if (origins_.count(kv.first)) return Error{"Animation bank merge collides at " + kv.first};
	size_t base = sources_.size();
	for (auto &kv : other.origins_) origins_.emplace(kv.first, base + kv.second);
	for (auto &s : other.sources_) sources_.push_back(std::move(s));
	// BTreeMap::extend: inserted keys replace existing ones (none collide after the check).
	for (auto &kv : other.clips_) clips_[kv.first] = std::move(kv.second);
	for (auto &kv : other.phase_blends_) phase_blends_[kv.first] = std::move(kv.second);
	for (auto &kv : other.blend_spaces_) blend_spaces_[kv.first] = std::move(kv.second);
	for (auto &kv : other.selectors_) selectors_[kv.first] = std::move(kv.second);
	for (auto &kv : other.selection_spaces_) selection_spaces_[kv.first] = std::move(kv.second);
	for (auto &kv : other.unsupported_) unsupported_[kv.first] = std::move(kv.second);
	return Unit{};
}

const BankSource *AnimationMetadata::source_for(const std::string &name) const
{
	auto it = origins_.find(to_ascii_uppercase(name));
	return it == origins_.end() ? nullptr : &sources_[it->second];
}

Result<const ClipMetadata *> AnimationMetadata::clip(const std::string &name_in) const
{
	std::string name = to_ascii_uppercase(name_in);
	S3_CHECK(validate_name(name, 36));
	// SetDBContent82D1B428..434 updates its separate clip map on every
	// clip record; repeated keys overwrite with the later bank record.
	auto it = clips_.find(name);
	if (it != clips_.end() && !it->second.empty()) {
		// Iterator::max_by_key returns the last maximum.
		const ClipMetadata *best = &it->second[0];
		for (const ClipMetadata &c : it->second)
			if (c.source_offset >= best->source_offset) best = &c;
		return best;
	}
	return Error{"Missing direct clip " + name};
}

Result<TreeMetadata> AnimationMetadata::tree(const std::string &name_in) const
{
	std::string name = to_ascii_uppercase(name_in);
	S3_CHECK(validate_name(name, 36));
	static const std::vector<ClipMetadata> no_clips;
	static const std::vector<PhaseBlendMetadata> no_phase;
	static const std::vector<BlendSpaceMetadata> no_blend;
	static const std::vector<SelectorMetadata> no_selectors;
	static const std::vector<SelectionSpaceMetadata> no_selection;
	static const std::vector<std::pair<uint64_t, uint32_t>> no_trees;
	auto pick = [&](const auto &map, const auto &empty) -> const auto & {
		auto it = map.find(name);
		return it == map.end() ? empty : it->second;
	};
	const auto &clips = pick(clips_, no_clips);
	const auto &trees = pick(unsupported_, no_trees);
	const auto &phase_blends = pick(phase_blends_, no_phase);
	const auto &blend_spaces = pick(blend_spaces_, no_blend);
	const auto &selectors = pick(selectors_, no_selectors);
	const auto &selection_spaces = pick(selection_spaces_, no_selection);
	// TU382D1B438..45C inserts into the tree map in file order and writes
	// the record pointer even when map[]82D27320 finds an existing key.
	bool any = false;
	uint64_t last = 0;
	auto see = [&](uint64_t v) {
		if (!any || v > last) last = v;
		any = true;
	};
	for (auto &c : clips) see(c.source_offset);
	for (auto &t : phase_blends) see(t.source_offset);
	for (auto &t : blend_spaces) see(t.source_offset);
	for (auto &t : selectors) see(t.source_offset);
	for (auto &t : selection_spaces) see(t.source_offset);
	for (auto &t : trees) see(t.first);
	if (any) {
		for (auto &c : clips)
			if (c.source_offset == last) return TreeMetadata(&c);
		for (auto &t : phase_blends)
			if (t.source_offset == last) return TreeMetadata(&t);
		for (auto &t : blend_spaces)
			if (t.source_offset == last) return TreeMetadata(&t);
		for (auto &t : selectors)
			if (t.source_offset == last) return TreeMetadata(&t);
		for (auto &t : selection_spaces)
			if (t.source_offset == last) return TreeMetadata(&t);
		for (auto &t : trees)
			if (t.first == last)
				return Error{"Animation " + name + " is tree type " + std::to_string(t.second) + "; tree evaluation is required"};
	}
	return Error{"Missing animation " + name + " in " + source_bank};
}

size_t AnimationMetadata::clip_count() const
{
	size_t n = 0;
	for (auto &kv : clips_) n += kv.second.size();
	return n;
}

} // namespace s3::data
