// Rust-semantics helpers shared by the skate-core port (`s3::rs`).
//
// Every Rust `f32` method used by skate-core maps to exactly one helper here so the
// port keeps binary32 behaviour (build with -ffp-contract=off). Pitfalls covered:
//   - `a.mul_add(b, c)`  -> rs::mul_add(a, b, c) == std::fma(a, b, c)  (a*b + c, one rounding)
//   - `f32::min/max`     -> IEEE minNum/maxNum: a NaN operand yields the other operand
//   - `f32::clamp`       -> NaN stays NaN; panics if min > max
//   - `f32::signum`      -> signum(+0.0) == 1.0, signum(-0.0) == -1.0, NaN -> NaN
//   - `f32::MIN`         -> -FLT_MAX (NOT FLT_MIN); `f32::MIN_POSITIVE` -> FLT_MIN
//   - `x as i32` etc.    -> saturating, NaN -> 0 (C++ casts are UB out of range)
//   - `iter().sum()`     -> starts from -0.0 (Rust >= 1.81)
//   - float literals must carry an `f` suffix: `x * 0.5` promotes to double in C++.
#pragma once

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace s3::rs {

// ---- panics -------------------------------------------------------------------------
struct Panic : std::logic_error {
    using std::logic_error::logic_error;
};
[[noreturn]] inline void panic(const char* what, const char* file, int line) {
    throw Panic(std::string(file) + ":" + std::to_string(line) + ": panic: " + what);
}
#define S3_ASSERT(cond)                                                    \
    do {                                                                   \
        if (!(cond)) ::s3::rs::panic("assertion failed: " #cond, __FILE__, __LINE__); \
    } while (0)
#define S3_PANIC(msg) ::s3::rs::panic(msg, __FILE__, __LINE__)

// ---- f32 constants ------------------------------------------------------------------
namespace f32 {
inline constexpr float EPSILON = FLT_EPSILON;
inline constexpr float MAX = FLT_MAX;
inline constexpr float MIN = -FLT_MAX;
inline constexpr float MIN_POSITIVE = FLT_MIN;
inline constexpr float INFINITY_ = std::numeric_limits<float>::infinity();
inline constexpr float NEG_INFINITY = -std::numeric_limits<float>::infinity();
inline const float NAN_ = std::numeric_limits<float>::quiet_NaN();
namespace consts {
inline constexpr float PI = 3.14159265358979323846264338327950288f;
inline constexpr float FRAC_PI_2 = 1.57079632679489661923132169163975144f;
inline constexpr float FRAC_PI_4 = 0.785398163397448309615660845819875721f;
inline constexpr float FRAC_1_SQRT_2 = 0.707106781186547524400844362104849039f;
inline constexpr float TAU = 6.28318530717958647692528676655900577f;
}  // namespace consts
}  // namespace f32

// ---- bits ---------------------------------------------------------------------------
inline float from_bits(std::uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}
inline std::uint32_t to_bits(float f) {
    std::uint32_t bits;
    std::memcpy(&bits, &f, sizeof bits);
    return bits;
}
/// `f32::to_be_bytes`.
inline std::array<std::uint8_t, 4> to_be_bytes(float f) {
    std::uint32_t b = to_bits(f);
    return {std::uint8_t(b >> 24), std::uint8_t(b >> 16), std::uint8_t(b >> 8), std::uint8_t(b)};
}
inline std::array<std::uint8_t, 4> to_be_bytes_u32(std::uint32_t b) {
    return {std::uint8_t(b >> 24), std::uint8_t(b >> 16), std::uint8_t(b >> 8), std::uint8_t(b)};
}

// ---- f32 methods --------------------------------------------------------------------
inline float mul_add(float a, float b, float c) { return std::fma(a, b, c); }
inline float abs(float x) { return std::fabs(x); }
inline float sqrt(float x) { return std::sqrt(x); }
inline float recip(float x) { return 1.0f / x; }
inline float min(float a, float b) { return std::fmin(a, b); }
inline float max(float a, float b) { return std::fmax(a, b); }
inline float clamp(float x, float lo, float hi) {
    if (!(lo <= hi)) panic("min > max, or either was NaN", __FILE__, __LINE__);
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    return x;
}
inline float signum(float x) { return std::isnan(x) ? x : std::copysign(1.0f, x); }
inline float copysign(float x, float s) { return std::copysign(x, s); }
inline float rem_euclid(float a, float b) {
    float r = std::fmod(a, b);
    return r < 0.0f ? r + std::fabs(b) : r;
}
inline float div_euclid(float a, float b) {
    float q = std::trunc(a / b);
    if (std::fmod(a, b) < 0.0f) return b > 0.0f ? q - 1.0f : q + 1.0f;
    return q;
}
inline float round_ties_even(float x) { return std::nearbyint(x); }
inline float round(float x) { return std::round(x); }
inline float floor(float x) { return std::floor(x); }
inline float ceil(float x) { return std::ceil(x); }
inline float trunc(float x) { return std::trunc(x); }
inline float fract(float x) { return x - std::trunc(x); }
inline bool is_finite(float x) { return std::isfinite(x); }
inline bool is_nan(float x) { return std::isnan(x); }
inline bool is_sign_negative(float x) { return std::signbit(x); }
inline bool is_sign_positive(float x) { return !std::signbit(x); }
inline float to_degrees(float x) { return x * 57.2957795130823208767981548141051703f; }
inline float to_radians(float x) {
    const float k = f32::consts::PI / 180.0f;
    return x * k;
}
/// `f32::powi`: compiler-rt `__powisf2` square-and-multiply.
inline float powi(float a, int b) {
    const bool recip_ = b < 0;
    float r = 1.0f;
    while (true) {
        if (b & 1) r *= a;
        b /= 2;
        if (b == 0) break;
        a *= a;
    }
    return recip_ ? 1.0f / r : r;
}
inline float powf(float a, float b) { return std::pow(a, b); }
inline float sin(float x) { return std::sin(x); }
inline float cos(float x) { return std::cos(x); }
inline float tan(float x) { return std::tan(x); }
inline float asin(float x) { return std::asin(x); }
inline float acos(float x) { return std::acos(x); }
inline float atan(float x) { return std::atan(x); }
inline float atan2(float y, float x) { return std::atan2(y, x); }
inline float exp(float x) { return std::exp(x); }
inline float ln(float x) { return std::log(x); }
inline float hypot(float a, float b) { return std::hypot(a, b); }

/// `f32::total_cmp` as a three-way result (-1, 0, 1).
inline int total_cmp(float a, float b) {
    std::int32_t l = std::int32_t(to_bits(a));
    std::int32_t r = std::int32_t(to_bits(b));
    l ^= std::int32_t(std::uint32_t(l >> 31) >> 1);
    r ^= std::int32_t(std::uint32_t(r >> 31) >> 1);
    return (l > r) - (l < r);
}

/// Rust float -> int `as` casts: saturating, NaN -> 0.
template <class I>
inline I as_int(float x) {
    if (std::isnan(x)) return I(0);
    if (x <= float(std::numeric_limits<I>::min())) return std::numeric_limits<I>::min();
    if (x >= float(std::numeric_limits<I>::max())) return std::numeric_limits<I>::max();
    return I(x);
}
inline std::int32_t as_i32(float x) { return as_int<std::int32_t>(x); }
inline std::uint32_t as_u32(float x) { return as_int<std::uint32_t>(x); }
inline std::size_t as_usize(float x) { return as_int<std::size_t>(x); }

/// `Iterator<Item = f32>::sum` (initial value -0.0, sequential adds).
template <class It>
inline float sum(It first, It last) {
    float s = -0.0f;
    for (; first != last; ++first) s += *first;
    return s;
}
template <class C>
inline float sum(const C& c) {
    return sum(std::begin(c), std::end(c));
}

// ---- arrays -------------------------------------------------------------------------
/// `std::array::from_fn`.
template <std::size_t N, class F>
inline auto from_fn(F&& f) {
    using T = std::decay_t<decltype(f(std::size_t(0)))>;
    std::array<T, N> out{};
    for (std::size_t i = 0; i < N; ++i) out[i] = f(i);
    return out;
}
/// `[T; N]::map`.
template <class T, std::size_t N, class F>
inline auto map(const std::array<T, N>& a, F&& f) {
    using U = std::decay_t<decltype(f(a[0]))>;
    std::array<U, N> out{};
    for (std::size_t i = 0; i < N; ++i) out[i] = f(a[i]);
    return out;
}
template <std::size_t N>
inline std::array<std::uint32_t, N> to_bits(const std::array<float, N>& a) {
    std::array<std::uint32_t, N> out{};
    for (std::size_t i = 0; i < N; ++i) out[i] = to_bits(a[i]);
    return out;
}
template <std::size_t N>
inline std::array<float, N> from_bits(const std::array<std::uint32_t, N>& a) {
    std::array<float, N> out{};
    for (std::size_t i = 0; i < N; ++i) out[i] = from_bits(a[i]);
    return out;
}

// ---- Result<T, E> -------------------------------------------------------------------
template <class E>
struct Err {
    E error;
};
template <class E>
inline Err<std::decay_t<E>> make_err(E&& e) {
    return {std::forward<E>(e)};
}
/// Minimal `Result<T, E>`: construct from a T (Ok) or from `rs::Err<E>`.
template <class T, class E>
class Result {
public:
    Result(const T& v) : v_(std::in_place_index<0>, v) {}
    Result(T&& v) : v_(std::in_place_index<0>, std::move(v)) {}
    template <class E2>
    Result(Err<E2> e) : v_(std::in_place_index<1>, E(std::move(e.error))) {}
    bool is_ok() const { return v_.index() == 0; }
    bool is_err() const { return v_.index() == 1; }
    const T& unwrap() const {
        if (!is_ok()) panic("called `Result::unwrap()` on an `Err` value", __FILE__, __LINE__);
        return std::get<0>(v_);
    }
    T& unwrap() {
        if (!is_ok()) panic("called `Result::unwrap()` on an `Err` value", __FILE__, __LINE__);
        return std::get<0>(v_);
    }
    const E& unwrap_err() const {
        if (!is_err()) panic("called `Result::unwrap_err()` on an `Ok` value", __FILE__, __LINE__);
        return std::get<1>(v_);
    }
    std::optional<T> ok() const { return is_ok() ? std::optional<T>(std::get<0>(v_)) : std::nullopt; }
    std::optional<E> err() const { return is_err() ? std::optional<E>(std::get<1>(v_)) : std::nullopt; }
    bool operator==(const Result& o) const { return v_ == o.v_; }
    bool operator!=(const Result& o) const { return !(v_ == o.v_); }

private:
    std::variant<T, E> v_;
};
/// `Result<(), E>`.
struct Unit {
    bool operator==(const Unit&) const { return true; }
    bool operator!=(const Unit&) const { return false; }
};

template <class T>
inline const T& unwrap(const std::optional<T>& o) {
    if (!o) panic("called `Option::unwrap()` on a `None` value", __FILE__, __LINE__);
    return *o;
}

}  // namespace s3::rs

// ---- derive(PartialEq) --------------------------------------------------------------
// S3_DERIVE_EQ(Type, field1, field2, ...) defines member-wise == / != (float ==: NaN != NaN,
// -0 == +0), matching Rust's derived PartialEq. Place it at namespace scope after the type.
#define S3_PP_CAT_(a, b) a##b
#define S3_PP_CAT(a, b) S3_PP_CAT_(a, b)
#define S3_PP_NARG(...) S3_PP_NARG_(__VA_ARGS__, S3_PP_RSEQ())
#define S3_PP_NARG_(...) S3_PP_ARG_N(__VA_ARGS__)
#define S3_PP_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                    _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32,  \
                    _33, _34, _35, _36, _37, _38, _39, _40, N, ...)                             \
    N
#define S3_PP_RSEQ()                                                                             \
    40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, \
        17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1
#define S3_EQF_1(f) (a.f == b.f)
#define S3_EQF_2(f, ...) (a.f == b.f) && S3_EQF_1(__VA_ARGS__)
#define S3_EQF_3(f, ...) (a.f == b.f) && S3_EQF_2(__VA_ARGS__)
#define S3_EQF_4(f, ...) (a.f == b.f) && S3_EQF_3(__VA_ARGS__)
#define S3_EQF_5(f, ...) (a.f == b.f) && S3_EQF_4(__VA_ARGS__)
#define S3_EQF_6(f, ...) (a.f == b.f) && S3_EQF_5(__VA_ARGS__)
#define S3_EQF_7(f, ...) (a.f == b.f) && S3_EQF_6(__VA_ARGS__)
#define S3_EQF_8(f, ...) (a.f == b.f) && S3_EQF_7(__VA_ARGS__)
#define S3_EQF_9(f, ...) (a.f == b.f) && S3_EQF_8(__VA_ARGS__)
#define S3_EQF_10(f, ...) (a.f == b.f) && S3_EQF_9(__VA_ARGS__)
#define S3_EQF_11(f, ...) (a.f == b.f) && S3_EQF_10(__VA_ARGS__)
#define S3_EQF_12(f, ...) (a.f == b.f) && S3_EQF_11(__VA_ARGS__)
#define S3_EQF_13(f, ...) (a.f == b.f) && S3_EQF_12(__VA_ARGS__)
#define S3_EQF_14(f, ...) (a.f == b.f) && S3_EQF_13(__VA_ARGS__)
#define S3_EQF_15(f, ...) (a.f == b.f) && S3_EQF_14(__VA_ARGS__)
#define S3_EQF_16(f, ...) (a.f == b.f) && S3_EQF_15(__VA_ARGS__)
#define S3_EQF_17(f, ...) (a.f == b.f) && S3_EQF_16(__VA_ARGS__)
#define S3_EQF_18(f, ...) (a.f == b.f) && S3_EQF_17(__VA_ARGS__)
#define S3_EQF_19(f, ...) (a.f == b.f) && S3_EQF_18(__VA_ARGS__)
#define S3_EQF_20(f, ...) (a.f == b.f) && S3_EQF_19(__VA_ARGS__)
#define S3_EQF_21(f, ...) (a.f == b.f) && S3_EQF_20(__VA_ARGS__)
#define S3_EQF_22(f, ...) (a.f == b.f) && S3_EQF_21(__VA_ARGS__)
#define S3_EQF_23(f, ...) (a.f == b.f) && S3_EQF_22(__VA_ARGS__)
#define S3_EQF_24(f, ...) (a.f == b.f) && S3_EQF_23(__VA_ARGS__)
#define S3_EQF_25(f, ...) (a.f == b.f) && S3_EQF_24(__VA_ARGS__)
#define S3_EQF_26(f, ...) (a.f == b.f) && S3_EQF_25(__VA_ARGS__)
#define S3_EQF_27(f, ...) (a.f == b.f) && S3_EQF_26(__VA_ARGS__)
#define S3_EQF_28(f, ...) (a.f == b.f) && S3_EQF_27(__VA_ARGS__)
#define S3_EQF_29(f, ...) (a.f == b.f) && S3_EQF_28(__VA_ARGS__)
#define S3_EQF_30(f, ...) (a.f == b.f) && S3_EQF_29(__VA_ARGS__)
#define S3_EQF_31(f, ...) (a.f == b.f) && S3_EQF_30(__VA_ARGS__)
#define S3_EQF_32(f, ...) (a.f == b.f) && S3_EQF_31(__VA_ARGS__)
#define S3_EQF_33(f, ...) (a.f == b.f) && S3_EQF_32(__VA_ARGS__)
#define S3_EQF_34(f, ...) (a.f == b.f) && S3_EQF_33(__VA_ARGS__)
#define S3_EQF_35(f, ...) (a.f == b.f) && S3_EQF_34(__VA_ARGS__)
#define S3_EQF_36(f, ...) (a.f == b.f) && S3_EQF_35(__VA_ARGS__)
#define S3_EQF_37(f, ...) (a.f == b.f) && S3_EQF_36(__VA_ARGS__)
#define S3_EQF_38(f, ...) (a.f == b.f) && S3_EQF_37(__VA_ARGS__)
#define S3_EQF_39(f, ...) (a.f == b.f) && S3_EQF_38(__VA_ARGS__)
#define S3_EQF_40(f, ...) (a.f == b.f) && S3_EQF_39(__VA_ARGS__)
#define S3_DERIVE_EQ(Type, ...)                                                                  \
    [[maybe_unused]] inline bool operator==(const Type& a, const Type& b) {                      \
        return S3_PP_CAT(S3_EQF_, S3_PP_NARG(__VA_ARGS__))(__VA_ARGS__);                         \
    }                                                                                            \
    [[maybe_unused]] inline bool operator!=(const Type& a, const Type& b) { return !(a == b); }
/// For field-less structs (unit structs) or structs with no comparable fields.
#define S3_DERIVE_EQ_EMPTY(Type)                                                                 \
    [[maybe_unused]] inline bool operator==(const Type&, const Type&) { return true; }           \
    [[maybe_unused]] inline bool operator!=(const Type&, const Type&) { return false; }
