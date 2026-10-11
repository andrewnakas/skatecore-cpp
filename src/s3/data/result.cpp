// Shared helpers for s3::data (Rust string/number semantics).
#include "result.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace s3::data {

std::string debug_str(const std::string &s)
{
	// Rust `<str as Debug>`: quotes, \" \\ \n \r \t \0, other controls as \u{..}.
	std::string out = "\"";
	for (unsigned char c : s) {
		switch (c) {
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		case '\0': out += "\\0"; break;
		default:
			if (c < 0x20 || c == 0x7f) {
				char b[16];
				std::snprintf(b, sizeof b, "\\u{%x}", c);
				out += b;
			} else {
				out += (char)c;
			}
		}
	}
	out += "\"";
	return out;
}

std::string to_ascii_uppercase(std::string s)
{
	for (char &c : s)
		if (c >= 'a' && c <= 'z') c = (char)(c - 32);
	return s;
}

bool eq_ignore_ascii_case(const std::string &a, const std::string &b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); i++) {
		char x = a[i], y = b[i];
		if (x >= 'a' && x <= 'z') x = (char)(x - 32);
		if (y >= 'a' && y <= 'z') y = (char)(y - 32);
		if (x != y) return false;
	}
	return true;
}

static bool ieq(const char *a, size_t n, const char *lit)
{
	if (std::strlen(lit) != n) return false;
	for (size_t i = 0; i < n; i++) {
		char x = a[i];
		if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
		if (x != lit[i]) return false;
	}
	return true;
}

bool parse_f32(const std::string &text, float &out)
{
	// Rust core::num::dec2flt grammar: [+-]? (inf | infinity | nan | Number)
	// Number = (Digit+ | Digit+ '.' Digit* | Digit* '.' Digit+) Exp?
	// Exp = [eE] [+-]? Digit+  (case-insensitive specials).
	const char *s = text.c_str();
	size_t n = text.size(), i = 0;
	if (n == 0) return false;
	bool negative = false;
	if (s[0] == '+' || s[0] == '-') {
		negative = s[0] == '-';
		i = 1;
	}
	const char *rest = s + i;
	size_t rn = n - i;
	if (ieq(rest, rn, "inf") || ieq(rest, rn, "infinity")) {
		out = negative ? -INFINITY : INFINITY;
		return true;
	}
	if (ieq(rest, rn, "nan")) {
		out = NAN;
		return true;
	}
	size_t digits = 0, j = i;
	while (j < n && s[j] >= '0' && s[j] <= '9') j++, digits++;
	if (j < n && s[j] == '.') {
		j++;
		while (j < n && s[j] >= '0' && s[j] <= '9') j++, digits++;
	}
	if (digits == 0) return false;
	if (j < n && (s[j] == 'e' || s[j] == 'E')) {
		j++;
		if (j < n && (s[j] == '+' || s[j] == '-')) j++;
		size_t e = 0;
		while (j < n && s[j] >= '0' && s[j] <= '9') j++, e++;
		if (e == 0) return false;
	}
	if (j != n) return false;
	// The text is plain decimal; strtof is correctly rounded (IEEE round-to-nearest).
	out = std::strtof(s, nullptr);
	return true;
}

template <class U>
static bool parse_radix(const std::string &text, unsigned radix, U &out)
{
	size_t i = 0;
	if (!text.empty() && text[0] == '+') i = 1;
	if (i >= text.size()) return false;
	U value = 0;
	for (; i < text.size(); i++) {
		char c = text[i];
		unsigned d;
		if (c >= '0' && c <= '9') d = (unsigned)(c - '0');
		else if (c >= 'a' && c <= 'z') d = (unsigned)(c - 'a' + 10);
		else if (c >= 'A' && c <= 'Z') d = (unsigned)(c - 'A' + 10);
		else return false;
		if (d >= radix) return false;
		U next = (U)(value * radix);
		if (next / radix != value) return false;
		if ((U)(next + d) < next) return false;
		value = (U)(next + d);
	}
	out = value;
	return true;
}

bool parse_u32_radix(const std::string &text, unsigned radix, uint32_t &out) { return parse_radix(text, radix, out); }
bool parse_u64_radix(const std::string &text, unsigned radix, uint64_t &out) { return parse_radix(text, radix, out); }

float f32_from_bits(uint32_t bits)
{
	float v;
	std::memcpy(&v, &bits, 4);
	return v;
}
uint32_t f32_to_bits(float v)
{
	uint32_t b;
	std::memcpy(&b, &v, 4);
	return b;
}
bool is_finite(float v) { return std::isfinite(v); }

} // namespace s3::data
