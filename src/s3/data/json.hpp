// Small strict JSON (RFC 8259) parser for the stock data files, no dependencies.
// The document is a flat node array; strings are decoded into one pool and numbers
// keep their source text so typed reads match serde_json (u32/u64/i32 exactness,
// integer-vs-float distinction). Object members keep file order (duplicates included).
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "result.hpp"

namespace s3::data {

enum class JsonKind : uint8_t { Null, Bool, Number, String, Array, Object };

class JsonDoc;

class JsonValue
{
public:
	JsonValue() = default;
	JsonValue(const JsonDoc *doc, uint32_t index) : doc_(doc), index_(index) {}
	bool valid() const { return doc_ != nullptr; }
	JsonKind kind() const;
	bool is_null() const { return kind() == JsonKind::Null; }
	bool is_bool() const { return kind() == JsonKind::Bool; }
	bool is_number() const { return kind() == JsonKind::Number; }
	bool is_string() const { return kind() == JsonKind::String; }
	bool is_array() const { return kind() == JsonKind::Array; }
	bool is_object() const { return kind() == JsonKind::Object; }
	bool as_bool() const;
	// Decoded string for String, source text for Number.
	std::string_view text() const;
	// Object member key (valid for children of objects).
	std::string_view key() const;
	// Child count for Array/Object.
	uint32_t size() const;
	JsonValue first_child() const;
	JsonValue next_sibling() const;
	// First member with this key (objects), or invalid.
	JsonValue get(std::string_view key) const;

	// Typed reads with serde_json semantics; `what` names the value in errors.
	Result<uint64_t> to_u64() const;
	Result<uint32_t> to_u32() const;
	Result<uint16_t> to_u16() const;
	Result<uint8_t> to_u8() const;
	Result<int64_t> to_i64() const;
	Result<int32_t> to_i32() const;
	Result<int16_t> to_i16() const;
	Result<double> to_f64() const;
	Result<float> to_f32() const;
	Result<bool> to_bool() const;
	Result<std::string> to_string() const;

	struct Iterator
	{
		const JsonDoc *doc;
		uint32_t index;
		JsonValue operator*() const { return JsonValue(doc, index); }
		Iterator &operator++();
		bool operator!=(const Iterator &o) const { return index != o.index; }
	};
	Iterator begin() const;
	Iterator end() const;

	std::string type_name() const;

private:
	const JsonDoc *doc_ = nullptr;
	uint32_t index_ = 0;
};

class JsonDoc
{
public:
	static constexpr uint32_t NONE = 0xffffffffu;
	struct Node
	{
		JsonKind kind;
		bool boolean;
		uint32_t next = NONE;   // next sibling
		uint32_t first = NONE;  // first child
		uint32_t count = 0;     // children
		uint32_t str_off = 0, str_len = 0;
		uint32_t key_off = 0, key_len = 0;
	};
	// Parse a complete document (trailing whitespace only). Errors use serde_json's
	// "... at line L column C" form.
	static Result<JsonDoc> parse(std::string_view text);
	JsonValue root() const { return JsonValue(this, 0); }

	std::vector<Node> nodes;
	std::string pool;
};

// serde-style struct decoding helper: tracks required/unknown/duplicate fields.
// Usage: JsonObject o(v, "File", true); o.field("version", [&](JsonValue x){...}); o.finish();
class JsonFields
{
public:
	// deny_unknown: serde(deny_unknown_fields).
	JsonFields(JsonValue object, std::vector<std::string_view> known, bool deny_unknown);
	// Error from construction (not an object, unknown/duplicate field).
	const std::string &error() const { return error_; }
	bool ok() const { return error_.empty(); }
	// The member for a field, or invalid if absent.
	JsonValue get(std::string_view name) const;
	// Required field: error "missing field `name`" if absent.
	Result<JsonValue> require(std::string_view name) const;

private:
	JsonValue object_;
	std::string error_;
};

// Typed collection helpers.
template <class T, class F>
Result<std::vector<T>> json_vec(JsonValue v, F &&element)
{
	if (!v.is_array()) return Error{"invalid type: " + v.type_name() + ", expected a sequence"};
	std::vector<T> out;
	out.reserve(v.size());
	for (JsonValue x : v) {
		Result<T> r = element(x);
		if (!r.ok) return Error{std::move(r.error)};
		out.push_back(std::move(r.value));
	}
	return out;
}
Result<std::vector<uint32_t>> json_u32_vec(JsonValue v);
Result<std::vector<int32_t>> json_i32_vec(JsonValue v);
Result<std::vector<std::string>> json_string_vec(JsonValue v);
// Fixed-size array `[u32; N]` (exact length).
Status json_u32_array(JsonValue v, uint32_t *out, size_t n);

} // namespace s3::data
