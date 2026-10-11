// Rust `Result<T, String>` for the s3::data loaders: `ok`, `value`, `error`.
// `return value;` builds an Ok, `return Error{"..."};` (or err("...")) an Err.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

namespace s3::data {

struct Error
{
	std::string message;
};
inline Error err(std::string message) { return Error{std::move(message)}; }

template <class T>
struct Result
{
	bool ok = false;
	T value{};
	std::string error;

	Result(T v) : ok(true), value(std::move(v)) {}
	Result(Error e) : ok(false), error(std::move(e.message)) {}
	explicit operator bool() const { return ok; }
	const T &operator*() const { return value; }
	T &operator*() { return value; }
	const T *operator->() const { return &value; }
	T *operator->() { return &value; }
};

struct Unit
{
};
using Status = Result<Unit>;
inline Status ok_status() { return Unit{}; }

// Rust `?`: on error return the error from the enclosing function.
#define S3_CONCAT_(a, b) a##b
#define S3_CONCAT(a, b) S3_CONCAT_(a, b)
#define S3_TRY_IMPL(tmp, decl, expr)                               \
	auto tmp = (expr);                                             \
	if (!tmp.ok) return ::s3::data::Error{std::move(tmp.error)};   \
	decl = std::move(tmp.value)
// S3_TRY(auto x, expr);  S3_TRY(x, expr) assigns an existing variable.
#define S3_TRY(decl, expr) S3_TRY_IMPL(S3_CONCAT(s3_try_, __LINE__), decl, expr)
// Discard the value: S3_CHECK(expr);
#define S3_CHECK(expr)                                                                         \
	do {                                                                                       \
		auto s3_check_tmp = (expr);                                                            \
		if (!s3_check_tmp.ok) return ::s3::data::Error{std::move(s3_check_tmp.error)};        \
	} while (0)
// Map the error message: S3_TRY_MAP(auto x, expr, [&](const std::string& e){ return ...; });
#define S3_TRY_MAP_IMPL(tmp, decl, expr, fn)                               \
	auto tmp = (expr);                                                     \
	if (!tmp.ok) return ::s3::data::Error{(fn)(tmp.error)};                \
	decl = std::move(tmp.value)
#define S3_TRY_MAP(decl, expr, fn) S3_TRY_MAP_IMPL(S3_CONCAT(s3_trym_, __LINE__), decl, expr, fn)

// Rust `{:#x}` / `{:X}` formatting helpers.
inline std::string hex_prefixed(uint64_t v)
{
	char b[32];
	std::snprintf(b, sizeof b, "0x%llx", (unsigned long long)v);
	return b;
}
inline std::string hex_upper(uint64_t v)
{
	char b[32];
	std::snprintf(b, sizeof b, "%llX", (unsigned long long)v);
	return b;
}

// Rust `{:?}` for str: quoted, with Rust's escapes.
std::string debug_str(const std::string &s);
// ASCII helpers matching Rust's str methods.
std::string to_ascii_uppercase(std::string s);
bool eq_ignore_ascii_case(const std::string &a, const std::string &b);
// Rust `str::parse::<f32>()`: Rust's grammar (no hex, no whitespace), correctly rounded.
bool parse_f32(const std::string &text, float &out);
// u32::from_str_radix / u64::from_str_radix (optional leading '+', no '-').
bool parse_u32_radix(const std::string &text, unsigned radix, uint32_t &out);
bool parse_u64_radix(const std::string &text, unsigned radix, uint64_t &out);
// Bit casts.
float f32_from_bits(uint32_t bits);
uint32_t f32_to_bits(float v);
bool is_finite(float v);

} // namespace s3::data
