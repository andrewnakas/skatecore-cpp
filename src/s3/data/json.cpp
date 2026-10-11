// Strict JSON parser and serde-like typed reads.
#include "json.hpp"
#include "vfs.hpp"

#include <cstdlib>
#include <cstring>
#include <limits>

namespace s3::data {

namespace {

struct Parser
{
	const char *s;
	size_t n;
	size_t at = 0;
	JsonDoc &doc;
	std::string error;
	int depth = 0;

	Parser(std::string_view text, JsonDoc &d) : s(text.data()), n(text.size()), doc(d) {}

	bool fail(const std::string &message)
	{
		if (error.empty()) {
			size_t line = 1, column = 0;
			for (size_t i = 0; i < at && i < n; i++) {
				if (s[i] == '\n') line++, column = 0;
				else column++;
			}
			if (at < n) column++;
			error = message + " at line " + std::to_string(line) + " column " + std::to_string(column);
		}
		return false;
	}

	void skip_ws()
	{
		while (at < n) {
			char c = s[at];
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r') at++;
			else break;
		}
	}

	static int hexval(char c)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	}

	bool read_hex4(uint32_t &out)
	{
		if (n - at < 4) {
			at = n;
			return fail("EOF while parsing a string");
		}
		out = 0;
		for (int i = 0; i < 4; i++) {
			int v = hexval(s[at]);
			if (v < 0) return fail("invalid escape");
			out = out << 4 | (uint32_t)v;
			at++;
		}
		return true;
	}

	static void put_utf8(std::string &o, uint32_t cp)
	{
		if (cp < 0x80) o += (char)cp;
		else if (cp < 0x800) {
			o += (char)(0xC0 | cp >> 6);
			o += (char)(0x80 | (cp & 0x3F));
		} else if (cp < 0x10000) {
			o += (char)(0xE0 | cp >> 12);
			o += (char)(0x80 | ((cp >> 6) & 0x3F));
			o += (char)(0x80 | (cp & 0x3F));
		} else {
			o += (char)(0xF0 | cp >> 18);
			o += (char)(0x80 | ((cp >> 12) & 0x3F));
			o += (char)(0x80 | ((cp >> 6) & 0x3F));
			o += (char)(0x80 | (cp & 0x3F));
		}
	}

	// Parses a string at s[at] == '"' and appends the decoded bytes to the pool.
	bool string(uint32_t &off, uint32_t &len)
	{
		at++; // opening quote
		off = (uint32_t)doc.pool.size();
		std::string &o = doc.pool;
		for (;;) {
			size_t start = at;
			while (at < n) {
				unsigned char c = (unsigned char)s[at];
				if (c == '"' || c == '\\' || c < 0x20) break;
				at++;
			}
			o.append(s + start, at - start);
			if (at >= n) return fail("EOF while parsing a string");
			char c = s[at];
			if (c == '"') {
				at++;
				break;
			}
			if (c != '\\') return fail("control character (\\u0000-\\u001F) found while parsing a string");
			at++;
			if (at >= n) return fail("EOF while parsing a string");
			char e = s[at++];
			switch (e) {
			case '"': o += '"'; break;
			case '\\': o += '\\'; break;
			case '/': o += '/'; break;
			case 'b': o += '\b'; break;
			case 'f': o += '\f'; break;
			case 'n': o += '\n'; break;
			case 'r': o += '\r'; break;
			case 't': o += '\t'; break;
			case 'u': {
				uint32_t cp;
				if (!read_hex4(cp)) return false;
				if (cp >= 0xDC00 && cp <= 0xDFFF) return fail("lone leading surrogate in hex escape");
				if (cp >= 0xD800 && cp <= 0xDBFF) {
					if (n - at < 2 || s[at] != '\\' || s[at + 1] != 'u') return fail("unexpected end of hex escape");
					at += 2;
					uint32_t lo;
					if (!read_hex4(lo)) return false;
					if (lo < 0xDC00 || lo > 0xDFFF) return fail("lone leading surrogate in hex escape");
					cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
				}
				put_utf8(o, cp);
				break;
			}
			default: return fail("invalid escape");
			}
		}
		len = (uint32_t)(o.size() - off);
		if (!is_utf8((const uint8_t *)o.data() + off, len)) return fail("invalid unicode code point");
		return true;
	}

	bool number(uint32_t node)
	{
		size_t start = at;
		if (s[at] == '-') at++;
		if (at >= n) return fail("EOF while parsing a value");
		if (s[at] == '0') {
			at++;
		} else if (s[at] >= '1' && s[at] <= '9') {
			while (at < n && s[at] >= '0' && s[at] <= '9') at++;
		} else {
			return fail("invalid number");
		}
		if (at < n && s[at] == '.') {
			at++;
			size_t d = at;
			while (at < n && s[at] >= '0' && s[at] <= '9') at++;
			if (at == d) return fail("invalid number");
		}
		if (at < n && (s[at] == 'e' || s[at] == 'E')) {
			at++;
			if (at < n && (s[at] == '+' || s[at] == '-')) at++;
			size_t d = at;
			while (at < n && s[at] >= '0' && s[at] <= '9') at++;
			if (at == d) return fail("invalid number");
		}
		JsonDoc::Node &nd = doc.nodes[node];
		nd.str_off = (uint32_t)doc.pool.size();
		nd.str_len = (uint32_t)(at - start);
		doc.pool.append(s + start, at - start);
		return true;
	}

	bool literal(const char *word)
	{
		size_t len = std::strlen(word);
		if (n - at < len || std::memcmp(s + at, word, len) != 0) {
			// serde_json reports EOF if the prefix matched up to the end.
			size_t k = 0;
			while (at + k < n && k < len && s[at + k] == word[k]) k++;
			at += k;
			if (at >= n) return fail("EOF while parsing a value");
			return fail("expected value");
		}
		at += len;
		return true;
	}

	uint32_t new_node(JsonKind kind)
	{
		JsonDoc::Node nd;
		nd.kind = kind;
		nd.boolean = false;
		doc.nodes.push_back(nd);
		return (uint32_t)(doc.nodes.size() - 1);
	}

	bool value(uint32_t &out)
	{
		skip_ws();
		if (at >= n) return fail("EOF while parsing a value");
		char c = s[at];
		switch (c) {
		case 'n':
			if (!literal("null")) return false;
			out = new_node(JsonKind::Null);
			return true;
		case 't':
			if (!literal("true")) return false;
			out = new_node(JsonKind::Bool);
			doc.nodes[out].boolean = true;
			return true;
		case 'f':
			if (!literal("false")) return false;
			out = new_node(JsonKind::Bool);
			return true;
		case '"': {
			out = new_node(JsonKind::String);
			uint32_t off, len;
			if (!string(off, len)) return false;
			doc.nodes[out].str_off = off;
			doc.nodes[out].str_len = len;
			return true;
		}
		case '[': return array(out);
		case '{': return object(out);
		default:
			if (c == '-' || (c >= '0' && c <= '9')) {
				out = new_node(JsonKind::Number);
				return number(out);
			}
			return fail("expected value");
		}
	}

	bool array(uint32_t &out)
	{
		if (++depth > 128) return fail("recursion limit exceeded");
		at++;
		out = new_node(JsonKind::Array);
		uint32_t last = JsonDoc::NONE, count = 0;
		skip_ws();
		if (at < n && s[at] == ']') {
			at++;
			depth--;
			return true;
		}
		for (;;) {
			uint32_t child;
			if (!value(child)) return false;
			if (last == JsonDoc::NONE) doc.nodes[out].first = child;
			else doc.nodes[last].next = child;
			last = child;
			count++;
			skip_ws();
			if (at >= n) return fail("EOF while parsing a list");
			if (s[at] == ',') {
				at++;
				skip_ws();
				if (at < n && s[at] == ']') return fail("trailing comma");
				continue;
			}
			if (s[at] == ']') {
				at++;
				break;
			}
			return fail("expected `,` or `]`");
		}
		doc.nodes[out].count = count;
		depth--;
		return true;
	}

	bool object(uint32_t &out)
	{
		if (++depth > 128) return fail("recursion limit exceeded");
		at++;
		out = new_node(JsonKind::Object);
		uint32_t last = JsonDoc::NONE, count = 0;
		skip_ws();
		if (at < n && s[at] == '}') {
			at++;
			depth--;
			return true;
		}
		for (;;) {
			skip_ws();
			if (at >= n) return fail("EOF while parsing an object");
			if (s[at] != '"') return fail("key must be a string");
			uint32_t koff, klen;
			if (!string(koff, klen)) return false;
			skip_ws();
			if (at >= n) return fail("EOF while parsing an object");
			if (s[at] != ':') return fail("expected `:`");
			at++;
			uint32_t child;
			if (!value(child)) return false;
			doc.nodes[child].key_off = koff;
			doc.nodes[child].key_len = klen;
			if (last == JsonDoc::NONE) doc.nodes[out].first = child;
			else doc.nodes[last].next = child;
			last = child;
			count++;
			skip_ws();
			if (at >= n) return fail("EOF while parsing an object");
			if (s[at] == ',') {
				at++;
				skip_ws();
				if (at < n && s[at] == '}') return fail("trailing comma");
				continue;
			}
			if (s[at] == '}') {
				at++;
				break;
			}
			return fail("expected `,` or `}`");
		}
		doc.nodes[out].count = count;
		depth--;
		return true;
	}
};

} // namespace

Result<JsonDoc> JsonDoc::parse(std::string_view text)
{
	JsonDoc doc;
	doc.nodes.reserve(text.size() / 16 + 16);
	doc.pool.reserve(text.size() / 2 + 16);
	Parser p(text, doc);
	uint32_t root;
	if (!p.value(root)) return Error{p.error};
	p.skip_ws();
	if (p.at != p.n) {
		p.fail("trailing characters");
		return Error{p.error};
	}
	return doc;
}

JsonKind JsonValue::kind() const { return doc_->nodes[index_].kind; }
bool JsonValue::as_bool() const { return doc_->nodes[index_].boolean; }
std::string_view JsonValue::text() const
{
	auto &nd = doc_->nodes[index_];
	return std::string_view(doc_->pool.data() + nd.str_off, nd.str_len);
}
std::string_view JsonValue::key() const
{
	auto &nd = doc_->nodes[index_];
	return std::string_view(doc_->pool.data() + nd.key_off, nd.key_len);
}
uint32_t JsonValue::size() const { return doc_->nodes[index_].count; }
JsonValue JsonValue::first_child() const
{
	uint32_t f = doc_->nodes[index_].first;
	return f == JsonDoc::NONE ? JsonValue() : JsonValue(doc_, f);
}
JsonValue JsonValue::next_sibling() const
{
	uint32_t f = doc_->nodes[index_].next;
	return f == JsonDoc::NONE ? JsonValue() : JsonValue(doc_, f);
}
JsonValue JsonValue::get(std::string_view key) const
{
	if (!is_object()) return JsonValue();
	for (JsonValue c : *this)
		if (c.key() == key) return c;
	return JsonValue();
}
JsonValue::Iterator &JsonValue::Iterator::operator++()
{
	index = doc->nodes[index].next;
	return *this;
}
JsonValue::Iterator JsonValue::begin() const { return Iterator{doc_, doc_->nodes[index_].first}; }
JsonValue::Iterator JsonValue::end() const { return Iterator{doc_, JsonDoc::NONE}; }

std::string JsonValue::type_name() const
{
	switch (kind()) {
	case JsonKind::Null: return "null";
	case JsonKind::Bool: return std::string("boolean `") + (as_bool() ? "true" : "false") + "`";
	case JsonKind::Number: {
		std::string t(text());
		bool integer = t.find_first_of(".eE") == std::string::npos;
		return (integer ? (t[0] == '-' ? "integer `" : "integer `") : "floating point `") + t + "`";
	}
	case JsonKind::String: return "string " + debug_str(std::string(text()));
	case JsonKind::Array: return "sequence";
	case JsonKind::Object: return "map";
	}
	return "value";
}

// Integer text without fraction/exponent, parsed into sign + magnitude.
static bool integer_parts(std::string_view t, bool &negative, uint64_t &magnitude)
{
	if (t.find_first_of(".eE") != std::string_view::npos) return false;
	negative = !t.empty() && t[0] == '-';
	size_t i = negative ? 1 : 0;
	uint64_t v = 0;
	for (; i < t.size(); i++) {
		uint64_t d = (uint64_t)(t[i] - '0');
		if (v > (std::numeric_limits<uint64_t>::max() - d) / 10) return false; // serde: becomes f64
		v = v * 10 + d;
	}
	magnitude = v;
	return true;
}

template <class T>
static Result<T> to_unsigned(const JsonValue &v, const char *name)
{
	if (!v.is_number()) return Error{"invalid type: " + v.type_name() + ", expected " + name};
	bool negative;
	uint64_t m;
	if (!integer_parts(v.text(), negative, m)) return Error{"invalid type: floating point `" + std::string(v.text()) + "`, expected " + name};
	if (negative && m != 0) return Error{"invalid value: integer `" + std::string(v.text()) + "`, expected " + name};
	if (m > (uint64_t)std::numeric_limits<T>::max()) return Error{"invalid value: integer `" + std::string(v.text()) + "`, expected " + name};
	return (T)m;
}

template <class T>
static Result<T> to_signed(const JsonValue &v, const char *name)
{
	if (!v.is_number()) return Error{"invalid type: " + v.type_name() + ", expected " + name};
	bool negative;
	uint64_t m;
	if (!integer_parts(v.text(), negative, m)) return Error{"invalid type: floating point `" + std::string(v.text()) + "`, expected " + name};
	int64_t value;
	if (negative) {
		if (m > (uint64_t)std::numeric_limits<int64_t>::max() + 1) return Error{"invalid type: floating point, expected " + std::string(name)};
		value = m == (uint64_t)std::numeric_limits<int64_t>::max() + 1 ? std::numeric_limits<int64_t>::min() : -(int64_t)m;
	} else {
		if (m > (uint64_t)std::numeric_limits<int64_t>::max()) return Error{"invalid value: integer `" + std::string(v.text()) + "`, expected " + name};
		value = (int64_t)m;
	}
	if (value < (int64_t)std::numeric_limits<T>::min() || value > (int64_t)std::numeric_limits<T>::max())
		return Error{"invalid value: integer `" + std::string(v.text()) + "`, expected " + name};
	return (T)value;
}

Result<uint64_t> JsonValue::to_u64() const { return to_unsigned<uint64_t>(*this, "u64"); }
Result<uint32_t> JsonValue::to_u32() const { return to_unsigned<uint32_t>(*this, "u32"); }
Result<uint16_t> JsonValue::to_u16() const { return to_unsigned<uint16_t>(*this, "u16"); }
Result<uint8_t> JsonValue::to_u8() const { return to_unsigned<uint8_t>(*this, "u8"); }
Result<int64_t> JsonValue::to_i64() const { return to_signed<int64_t>(*this, "i64"); }
Result<int32_t> JsonValue::to_i32() const { return to_signed<int32_t>(*this, "i32"); }
Result<int16_t> JsonValue::to_i16() const { return to_signed<int16_t>(*this, "i16"); }
Result<double> JsonValue::to_f64() const
{
	if (!is_number()) return Error{"invalid type: " + type_name() + ", expected f64"};
	std::string t(text());
	return std::strtod(t.c_str(), nullptr);
}
Result<float> JsonValue::to_f32() const
{
	// serde_json parses an f64 and narrows it (`as f32`).
	if (!is_number()) return Error{"invalid type: " + type_name() + ", expected f32"};
	std::string t(text());
	return (float)std::strtod(t.c_str(), nullptr);
}
Result<bool> JsonValue::to_bool() const
{
	if (!is_bool()) return Error{"invalid type: " + type_name() + ", expected a boolean"};
	return as_bool();
}
Result<std::string> JsonValue::to_string() const
{
	if (!is_string()) return Error{"invalid type: " + type_name() + ", expected a string"};
	return std::string(text());
}

JsonFields::JsonFields(JsonValue object, std::vector<std::string_view> known, bool deny_unknown) : object_(object)
{
	if (!object.is_object()) {
		error_ = "invalid type: " + object.type_name() + ", expected struct";
		return;
	}
	std::vector<bool> seen(known.size(), false);
	for (JsonValue m : object) {
		std::string_view k = m.key();
		size_t i = 0;
		for (; i < known.size(); i++)
			if (known[i] == k) break;
		if (i == known.size()) {
			if (deny_unknown) {
				std::string list;
				for (size_t j = 0; j < known.size(); j++) list += (j ? ", `" : "`") + std::string(known[j]) + "`";
				error_ = "unknown field `" + std::string(k) + "`, expected " + (known.size() == 1 ? list : "one of " + list);
				return;
			}
			continue;
		}
		if (seen[i]) {
			error_ = "duplicate field `" + std::string(k) + "`";
			return;
		}
		seen[i] = true;
	}
}

JsonValue JsonFields::get(std::string_view name) const { return object_.get(name); }

Result<JsonValue> JsonFields::require(std::string_view name) const
{
	JsonValue v = object_.get(name);
	if (!v.valid()) return Error{"missing field `" + std::string(name) + "`"};
	return v;
}

Result<std::vector<uint32_t>> json_u32_vec(JsonValue v)
{
	return json_vec<uint32_t>(v, [](JsonValue x) { return x.to_u32(); });
}
Result<std::vector<int32_t>> json_i32_vec(JsonValue v)
{
	return json_vec<int32_t>(v, [](JsonValue x) { return x.to_i32(); });
}
Result<std::vector<std::string>> json_string_vec(JsonValue v)
{
	return json_vec<std::string>(v, [](JsonValue x) { return x.to_string(); });
}
Status json_u32_array(JsonValue v, uint32_t *out, size_t n)
{
	if (!v.is_array()) return Error{"invalid type: " + v.type_name() + ", expected an array of length " + std::to_string(n)};
	if (v.size() != n) return Error{"invalid length " + std::to_string(v.size()) + ", expected an array of length " + std::to_string(n)};
	size_t i = 0;
	for (JsonValue x : v) {
		S3_TRY(out[i++], x.to_u32());
	}
	return Unit{};
}

} // namespace s3::data
