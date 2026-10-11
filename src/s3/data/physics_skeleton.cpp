// Lossless stock physics-bone records (physics_skeleton.rs).
#include "physics_skeleton.hpp"

#include <set>

#include "json.hpp"

namespace s3::data {

std::array<float, 3> PhysicsBone::size() const
{
	return {f32_from_bits(words[8]), f32_from_bits(words[9]), f32_from_bits(words[10])};
}
std::array<float, 4> PhysicsBone::rotation() const
{
	return {f32_from_bits(words[12]), f32_from_bits(words[13]), f32_from_bits(words[14]), f32_from_bits(words[15])};
}
std::array<float, 3> PhysicsBone::translation() const
{
	return {f32_from_bits(words[16]), f32_from_bits(words[17]), f32_from_bits(words[18])};
}

static Result<PhysicsBone> parse_bone(JsonValue v)
{
	JsonFields f(v, {"name", "source_offset", "words"}, true);
	if (!f.ok()) return Error{f.error()};
	PhysicsBone b;
	S3_TRY(JsonValue name, f.require("name"));
	S3_TRY(b.name, name.to_string());
	S3_TRY(JsonValue offset, f.require("source_offset"));
	S3_TRY(b.source_offset, offset.to_u64());
	S3_TRY(JsonValue words, f.require("words"));
	S3_CHECK(json_u32_array(words, b.words.data(), 28));
	return b;
}

static Result<PhysicsSkeleton> parse_skeleton(JsonValue v)
{
	JsonFields f(v, {"name", "source_offset", "bones"}, true);
	if (!f.ok()) return Error{f.error()};
	PhysicsSkeleton s;
	S3_TRY(JsonValue name, f.require("name"));
	S3_TRY(s.name, name.to_string());
	S3_TRY(JsonValue offset, f.require("source_offset"));
	S3_TRY(s.source_offset, offset.to_u64());
	S3_TRY(JsonValue bones, f.require("bones"));
	S3_TRY(s.bones, json_vec<PhysicsBone>(bones, parse_bone));
	return s;
}

Result<PhysicsSkeleton> PhysicsSkeleton::parse(std::string_view json, const std::string &bank_sha256, const std::string &name)
{
	S3_TRY(JsonDoc doc, JsonDoc::parse(json));
	JsonFields f(doc.root(), {"version", "source_sha256", "skeletons"}, true);
	if (!f.ok()) return Error{f.error()};
	S3_TRY(JsonValue version_v, f.require("version"));
	S3_TRY(uint32_t version, version_v.to_u32());
	S3_TRY(JsonValue sha_v, f.require("source_sha256"));
	S3_TRY(std::string source_sha256, sha_v.to_string());
	S3_TRY(JsonValue list, f.require("skeletons"));
	S3_TRY(std::vector<PhysicsSkeleton> skeletons, json_vec<PhysicsSkeleton>(list, parse_skeleton));
	if (version != 1 || source_sha256 != bank_sha256)
		return Error{"Physics skeleton bank identity or format differs from animation data"};
	std::set<std::string> identities;
	for (const PhysicsSkeleton &s : skeletons)
		if (!identities.insert(s.name).second) return Error{"Converted physics skeleton has unresolved duplicate names"};
	PhysicsSkeleton *selected = nullptr;
	for (PhysicsSkeleton &s : skeletons) {
		if (eq_ignore_ascii_case(s.name, name)) {
			selected = &s;
			break;
		}
	}
	if (!selected) return Error{"Required stock physics skeleton " + name + " is absent"};
	if (selected->bones.empty() || selected->bones.size() > 255) return Error{"Invalid stock physical bone count"};
	std::set<std::string> names;
	for (const PhysicsBone &bone : selected->bones) {
		bool bad = !names.insert(bone.name).second;
		if (!bad) {
			for (float v : bone.size()) bad |= !is_finite(v);
			for (float v : bone.rotation()) bad |= !is_finite(v);
			for (float v : bone.translation()) bad |= !is_finite(v);
		}
		if (bad) return Error{"Invalid or repeated stock physics bone " + bone.name};
	}
	return std::move(*selected);
}

Result<PhysicsSkeleton> PhysicsSkeleton::load(const Vfs &vfs, const std::string &path, const std::string &bank_sha256, const std::string &name)
{
	S3_TRY_MAP(SharedBytes bytes, vfs.read(path), [&](const std::string &e) { return path + ": " + e; });
	return parse(std::string_view((const char *)bytes.data, bytes.size), bank_sha256, name);
}

} // namespace s3::data
