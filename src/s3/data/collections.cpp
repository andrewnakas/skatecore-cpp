// Typed access to converted stock skater XML values (collections.rs).
#include "collections.hpp"

#include <set>

#include "attrib_hash.hpp"
#include "json.hpp"

namespace s3::data {

static Result<FieldArray> parse_field_array(JsonValue v)
{
	JsonFields f(v, {"capacity", "element_size", "alignment", "items"}, false);
	if (!f.ok()) return Error{f.error()};
	FieldArray a;
	S3_TRY(JsonValue capacity, f.require("capacity"));
	S3_TRY(a.capacity, capacity.to_u32());
	S3_TRY(JsonValue element_size, f.require("element_size"));
	S3_TRY(a.element_size, element_size.to_u32());
	S3_TRY(JsonValue alignment, f.require("alignment"));
	S3_TRY(a.alignment, alignment.to_u32());
	S3_TRY(JsonValue items, f.require("items"));
	S3_TRY(a.items, json_string_vec(items));
	return a;
}

static Result<Field> parse_field(JsonValue v)
{
	JsonFields f(v, {"type", "data", "array"}, false);
	if (!f.ok()) return Error{f.error()};
	Field out;
	S3_TRY(JsonValue type, f.require("type"));
	S3_TRY(out.type_name, type.to_string());
	S3_TRY(JsonValue data, f.require("data"));
	S3_TRY(out.data, data.to_string());
	JsonValue array = f.get("array");
	if (array.valid() && !array.is_null()) {
		S3_TRY(FieldArray a, parse_field_array(array));
		out.array = std::move(a);
	}
	return out;
}

static Result<Collection> parse_collection(JsonValue v)
{
	JsonFields f(v, {"class", "key", "parent", "fields", "source", "sha256"}, false);
	if (!f.ok()) return Error{f.error()};
	Collection c;
	S3_TRY(JsonValue cls, f.require("class"));
	S3_TRY(c.class_name, cls.to_string());
	S3_TRY(JsonValue key, f.require("key"));
	S3_TRY(c.key, key.to_string());
	S3_TRY(JsonValue parent, f.require("parent"));
	S3_TRY(c.parent, parent.to_string());
	S3_TRY(JsonValue fields, f.require("fields"));
	if (!fields.is_object()) return Error{"invalid type: " + fields.type_name() + ", expected a map"};
	for (JsonValue m : fields) {
		S3_TRY(Field field, parse_field(m));
		// BTreeMap deserialization: a repeated key keeps the last value.
		c.fields[std::string(m.key())] = std::move(field);
	}
	S3_TRY(JsonValue source, f.require("source"));
	S3_TRY(c.source, source.to_string());
	S3_TRY(JsonValue sha, f.require("sha256"));
	S3_TRY(c.sha256, sha.to_string());
	c.class_id = attrib_hash::numeric_name(c.class_name);
	c.key_id = attrib_hash::numeric_name(c.key);
	return c;
}

Result<Collections> Collections::from_json(std::string_view text)
{
	S3_TRY(JsonDoc doc, JsonDoc::parse(text));
	JsonFields f(doc.root(), {"version", "collections"}, false);
	if (!f.ok()) return Error{f.error()};
	Collections out;
	S3_TRY(JsonValue version, f.require("version"));
	S3_TRY(out.version, version.to_u32());
	S3_TRY(JsonValue list, f.require("collections"));
	S3_TRY(out.collections, json_vec<Collection>(list, parse_collection));
	return out;
}

Result<Collections> Collections::load(const Vfs &vfs, const std::string &asset_root)
{
	std::string path = join_path(asset_root, "private/stock/skater-collections.json");
	S3_TRY_MAP(SharedBytes bytes, vfs.read(path), [&](const std::string &e) { return path + ": " + e; });
	S3_TRY(Collections data, from_json(std::string_view((const char *)bytes.data, bytes.size)));
	if (data.version != 1) return Error{"Unsupported skater collections version " + std::to_string(data.version)};
	std::set<std::pair<std::string, std::string>> identities;
	for (const Collection &item : data.collections) {
		if (!identities.insert({item.class_id, item.key_id}).second)
			return Error{"Duplicate collection " + item.class_name + "/" + item.key};
	}
	return data;
}

Result<const Field *> Collections::field(const std::string &cls, const std::string &key, const std::string &name) const
{
	// Converted vaults can retain numeric identities when debug names were
	// unavailable. Both spellings identify the same native AttribSys entry.
	std::string class_id = attrib_hash::numeric_name(cls);
	std::string field_id = attrib_hash::numeric_name(name);
	const std::string *current = &key;
	for (size_t step = 0; step <= collections.size(); step++) {
		std::string current_id = attrib_hash::numeric_name(*current);
		// `c.class_name == class || numeric(c.class_name) == class_id` reduces to the
		// identity comparison (equal names have equal identities).
		const Collection *item = nullptr;
		for (const Collection &c : collections) {
			if (c.class_id == class_id && c.key_id == current_id) {
				item = &c;
				break;
			}
		}
		if (!item) return Error{"Missing stock collection " + cls + "/" + *current};
		auto exact = item->fields.find(name);
		if (exact != item->fields.end()) return &exact->second;
		for (const auto &kv : item->fields)
			if (attrib_hash::numeric_name(kv.first) == field_id) return &kv.second;
		if (item->parent.empty()) return Error{"Missing stock field " + cls + "/" + key + "/" + name};
		current = &item->parent;
	}
	return Error{"Cyclic stock collection inheritance " + cls + "/" + key};
}

Status decode_words(const std::string &text, uint32_t *out, size_t n)
{
	std::string hex;
	bool ascii = true;
	for (unsigned char c : text) {
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') continue;
		if (c >= 0x80) ascii = false;
		hex += (char)c;
	}
	if (hex.size() != n * 8 || !ascii)
		return Error{"Expected " + std::to_string(n) + " big-endian words, found " + std::to_string(hex.size()) + " bytes of hex"};
	for (size_t i = 0; i < n; i++) {
		if (!parse_u32_radix(hex.substr(i * 8, 8), 16, out[i])) return Error{"Invalid collection payload: invalid digit found in string"};
	}
	return Unit{};
}

Result<float> Collections::float_(const std::string &cls, const std::string &key, const std::string &name) const
{
	S3_TRY(const Field *field, this->field(cls, key, name));
	if (field->type_name != "EA::Reflection::Float") return Error{"Expected float at " + cls + "/" + key + "/" + name};
	uint32_t word;
	S3_CHECK(decode_words(field->data, &word, 1));
	float value = f32_from_bits(word);
	if (!is_finite(value)) return Error{"Non-finite stock float " + cls + "/" + key + "/" + name};
	return value;
}

Result<uint32_t> Collections::integer(const std::string &cls, const std::string &key, const std::string &name) const
{
	S3_TRY(const Field *field, this->field(cls, key, name));
	if (field->type_name != "EA::Reflection::Int32" && field->type_name != "EA::Reflection::UInt32")
		return Error{"Expected integer at " + cls + "/" + key + "/" + name};
	uint32_t word;
	S3_CHECK(decode_words(field->data, &word, 1));
	return word;
}

Result<bool> Collections::boolean(const std::string &cls, const std::string &key, const std::string &name) const
{
	S3_TRY(const Field *field, this->field(cls, key, name));
	if (field->type_name != "EA::Reflection::Bool") return Error{"Expected boolean at " + cls + "/" + key + "/" + name};
	// Attributes store the byte followed by padding; layout bools may
	// contain only the byte. Reading a big-endian u32 gives the wrong bit.
	// (`str::get(..2)` is None when byte 2 is not a char boundary.)
	const std::string &d = field->data;
	bool boundary = d.size() >= 2 && (d.size() == 2 || ((unsigned char)d[2] & 0xC0) != 0x80);
	if (boundary && d.compare(0, 2, "00") == 0) return false;
	if (boundary && d.compare(0, 2, "01") == 0) return true;
	return Error{"Invalid stock boolean " + cls + "/" + key + "/" + name};
}

Result<std::vector<float>> Collections::float_items(const std::string &cls, const std::string &key, const std::string &name) const
{
	S3_TRY(const Field *field, this->field(cls, key, name));
	if (field->type_name != "EA::Reflection::Float") return Error{"Expected float array at " + cls + "/" + key + "/" + name};
	if (!field->array) return Error{"Expected array payload at " + cls + "/" + key + "/" + name};
	std::vector<float> out;
	for (const std::string &item : field->array->items) {
		uint32_t word;
		S3_CHECK(decode_words(item, &word, 1));
		float value = f32_from_bits(word);
		if (!is_finite(value)) return Error{"Non-finite stock float " + cls + "/" + key + "/" + name};
		out.push_back(value);
	}
	return out;
}

} // namespace s3::data
