// Shared helpers for the skate-game simulation port (`s3::game`).
//
//   sc::   alias of ::s3 (skate-core port: sc::physics::board::BodyId, sc::player::state::...)
//   sd::   alias of ::s3::data (skate-data loaders)
//   Status                  Rust `Result<(), String>`;  `return ok();` / `return rs::make_err(msg);`
//   S3G_TRY(decl, expr)     Rust `let decl = expr?;` for rs::Result<T, std::string> AND s3::data::Result<T>
//   S3G_CHECK(expr)         Rust `expr?;` (value discarded)
//   S3G_TRY_OPT(decl, opt, msg)  Rust `let decl = opt.ok_or(msg)?;` (std::optional or pointer)
//   log::info / log::error  logshim (stderr; quiet unless S3_GAME_LOG is set)
//   fmt::debug_f32(x)       Rust `{:?}` of an f32 (shortest round-trip), fmt::display_f32 for `{}`
//   fmt::cat(a, b, ...)     string concatenation for format! messages
#pragma once
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>

#include "s3/core/rs.hpp"
#include "s3/data/result.hpp"

namespace s3::game {

namespace sc = ::s3;
namespace sd = ::s3::data;
namespace rs = ::s3::rs;

using Status = rs::Result<rs::Unit, std::string>;
inline Status ok() { return rs::Unit{}; }

namespace detail {
template <class T, class E>
inline bool is_err(const rs::Result<T, E>& r) { return r.is_err(); }
template <class T>
inline bool is_err(const sd::Result<T>& r) { return !r.ok; }
template <class T, class E>
inline std::string err_string(const rs::Result<T, E>& r) {
    if constexpr (std::is_convertible_v<const E&, std::string>) {
        return std::string(r.unwrap_err());
    } else {
        return r.unwrap_err().to_string();
    }
}
template <class T>
inline std::string err_string(const sd::Result<T>& r) { return r.error; }
template <class T, class E>
inline T&& take(rs::Result<T, E>& r) { return std::move(r.unwrap()); }
template <class T>
inline T&& take(sd::Result<T>& r) { return std::move(r.value); }
template <class T>
inline bool is_none(const std::optional<T>& o) { return !o.has_value(); }
template <class T>
inline bool is_none(T* p) { return p == nullptr; }
template <class T>
inline T&& take_opt(std::optional<T>& o) { return std::move(*o); }
template <class T>
inline T* take_opt(T* p) { return p; }
}  // namespace detail

}  // namespace s3::game

#define S3G_CAT_(a, b) a##b
#define S3G_CAT(a, b) S3G_CAT_(a, b)
#define S3G_TRY_IMPL(tmp, decl, expr)                                                       \
    auto tmp = (expr);                                                                      \
    if (::s3::game::detail::is_err(tmp))                                                    \
        return ::s3::rs::make_err(std::string(::s3::game::detail::err_string(tmp)));        \
    decl = ::s3::game::detail::take(tmp)
#define S3G_TRY(decl, expr) S3G_TRY_IMPL(S3G_CAT(s3g_try_, __LINE__), decl, expr)
#define S3G_CHECK(expr)                                                                     \
    do {                                                                                    \
        auto s3g_check_ = (expr);                                                           \
        if (::s3::game::detail::is_err(s3g_check_))                                         \
            return ::s3::rs::make_err(std::string(::s3::game::detail::err_string(s3g_check_))); \
    } while (0)
#define S3G_TRY_OPT_IMPL(tmp, decl, opt, msg)                                               \
    auto&& tmp = (opt);                                                                     \
    if (::s3::game::detail::is_none(tmp)) return ::s3::rs::make_err(std::string(msg));      \
    decl = ::s3::game::detail::take_opt(tmp)
#define S3G_TRY_OPT(decl, opt, msg) S3G_TRY_OPT_IMPL(S3G_CAT(s3g_opt_, __LINE__), decl, opt, msg)

namespace s3::game::log {
inline bool enabled() {
    static const bool on = std::getenv("S3_GAME_LOG") != nullptr;
    return on;
}
inline void info(const std::string& msg) {
    if (enabled()) std::fprintf(stderr, "INFO %s\n", msg.c_str());
}
inline void error(const std::string& msg) {
    if (enabled()) std::fprintf(stderr, "ERROR %s\n", msg.c_str());
}
/// `eprintln!` (always printed, like the Rust host).
inline void eprintln(const std::string& msg) { std::fprintf(stderr, "%s\n", msg.c_str()); }
}  // namespace s3::game::log

namespace s3::game::fmt {

namespace detail {
/// Shortest round-trip digits and decimal exponent of a finite, non-zero |x|:
/// x = 0.d1d2d3... * 10^exp10  (digits has no leading/trailing zeros).
inline void shortest(float x, std::string& digits, int& exp10) {
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof buf, x, std::chars_format::scientific);
    std::string s(buf, res.ptr);
    // s = "d.ddddde[+-]XX"
    const auto e = s.find('e');
    std::string mant = s.substr(0, e);
    const int ex = std::atoi(s.c_str() + e + 1);
    digits.clear();
    for (char c : mant)
        if (c >= '0' && c <= '9') digits.push_back(c);
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    exp10 = ex + 1;
    // Exact ties: libc++ picks the even last digit, Rust's shortest (Grisu/Dragon) rounds the
    // tie up. Detect "exact value == digits + half a unit in the last place" and round up.
    char exact_buf[160];
    std::snprintf(exact_buf, sizeof exact_buf, "%.120e", double(x));
    std::string exact;
    for (const char* c = exact_buf; *c && *c != 'e'; ++c)
        if (*c >= '0' && *c <= '9') exact.push_back(*c);
    while (exact.size() > 1 && exact.back() == '0') exact.pop_back();
    const int exact_exp = std::atoi(std::strchr(exact_buf, 'e') + 1) + 1;
    if (exact_exp == exp10 && exact.size() == digits.size() + 1 && exact.back() == '5' &&
        exact.compare(0, digits.size(), digits) == 0) {
        int i = int(digits.size()) - 1;
        while (i >= 0 && digits[std::size_t(i)] == '9') {
            digits[std::size_t(i)] = '0';
            --i;
        }
        if (i < 0) {
            digits.insert(digits.begin(), '1');
            exp10 += 1;
        } else {
            digits[std::size_t(i)] = char(digits[std::size_t(i)] + 1);
        }
        while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    }
}
/// core::fmt::float `float_to_decimal_common_shortest` with `min_precision` fraction digits.
inline std::string decimal(float x, std::size_t min_precision) {
    if (x == 0.0f) {
        std::string out = std::signbit(x) ? "-0" : "0";
        if (min_precision > 0) out += "." + std::string(min_precision, '0');
        return out;
    }
    std::string digits;
    int exp10 = 0;
    shortest(std::fabs(x), digits, exp10);
    std::string out = std::signbit(x) ? "-" : "";
    const int n = int(digits.size());
    std::string frac;
    if (exp10 <= 0) {
        out += "0";
        frac = std::string(std::size_t(-exp10), '0') + digits;
    } else if (exp10 >= n) {
        out += digits + std::string(std::size_t(exp10 - n), '0');
    } else {
        out += digits.substr(0, std::size_t(exp10));
        frac = digits.substr(std::size_t(exp10));
    }
    if (frac.size() < min_precision) frac += std::string(min_precision - frac.size(), '0');
    if (!frac.empty()) out += "." + frac;
    return out;
}
/// `float_to_exponential_common_shortest` (lower-case `e`, no `+`).
inline std::string exponential(float x) {
    std::string digits;
    int exp10 = 0;
    shortest(std::fabs(x), digits, exp10);
    std::string out = std::signbit(x) ? "-" : "";
    out += digits.substr(0, 1);
    if (digits.size() > 1) out += "." + digits.substr(1);
    out += "e" + std::to_string(exp10 - 1);
    return out;
}
inline const char* special(float x) {
    if (std::isnan(x)) return "NaN";
    if (std::isinf(x)) return x > 0.0f ? "inf" : "-inf";
    return nullptr;
}
}  // namespace detail

/// Rust `format!("{:?}", x)` for f32.
inline std::string debug_f32(float x) {
    if (const char* s = detail::special(x)) return s;
    const float a = std::fabs(x);
    if (a != 0.0f && (a < 1e-4f || a >= 1e16f)) return detail::exponential(x);
    return detail::decimal(x, 1);
}
/// Rust `format!("{}", x)` for f32.
inline std::string display_f32(float x) {
    if (const char* s = detail::special(x)) return s;
    return detail::decimal(x, 0);
}

inline void cat_one(std::ostringstream& os, const std::string& v) { os << v; }
inline void cat_one(std::ostringstream& os, const char* v) { os << v; }
inline void cat_one(std::ostringstream& os, float v) { os << display_f32(v); }
inline void cat_one(std::ostringstream& os, double v) { os << v; }
inline void cat_one(std::ostringstream& os, bool v) { os << (v ? "true" : "false"); }
template <class T>
inline std::enable_if_t<std::is_integral_v<T>> cat_one(std::ostringstream& os, T v) {
    os << +v;
}
template <class... A>
inline std::string cat(const A&... a) {
    std::ostringstream os;
    (cat_one(os, a), ...);
    return os.str();
}

}  // namespace s3::game::fmt
