// Tiny test harness for the skate-core port (no external deps).
//
//   S3_TEST(name) { ... }        registers a test (port of a Rust #[test]); internal linkage.
//   CHECK(cond)                  Rust assert!(cond)
//   CHECK_EQ(a, b)               Rust assert_eq!(a, b): uses operator== (f32 == is exact, NaN != NaN)
//   CHECK_NE(a, b)               Rust assert_ne!
//   CHECK_BITS_EQ(a, b)          compare f32 bit patterns (to_bits)
//   CHECK_PANICS(expr)           Rust #[should_panic] / catch_unwind is_err
// A failed check throws, ending the test (like a Rust panic); the runner reports and continues.
#pragma once
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "s3/core/rs.hpp"

namespace s3test {

struct Failure {
    std::string message;
};

using TestFn = void (*)();
struct TestCase {
    const char* file;
    const char* name;
    TestFn fn;
};
std::vector<TestCase>& registry();

struct Registrar {
    Registrar(const char* file, const char* name, TestFn fn) { registry().push_back({file, name, fn}); }
};

template <class T, class = void>
struct is_streamable : std::false_type {};
template <class T>
struct is_streamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

template <class T, class = void>
struct is_iterable : std::false_type {};
template <class T>
struct is_iterable<T, std::void_t<decltype(std::begin(std::declval<const T&>())),
                                  decltype(std::end(std::declval<const T&>()))>> : std::true_type {};

template <class T>
void show(std::ostream& os, const T& v);

inline void show_float(std::ostream& os, float f) {
    os << std::setprecision(9) << f << " (0x" << std::hex << std::setw(8) << std::setfill('0')
       << s3::rs::to_bits(f) << std::dec << std::setfill(' ') << ")";
}

template <class T>
void show(std::ostream& os, const T& v) {
    if constexpr (std::is_same_v<T, float>) {
        show_float(os, v);
    } else if constexpr (std::is_same_v<T, double>) {
        os << std::setprecision(17) << v;
    } else if constexpr (std::is_same_v<T, bool>) {
        os << (v ? "true" : "false");
    } else if constexpr (std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::int8_t>) {
        os << int(v);
    } else if constexpr (std::is_enum_v<T>) {
        os << "enum(" << static_cast<long long>(v) << ")";
    } else if constexpr (std::is_same_v<T, std::string> || std::is_convertible_v<T, const char*>) {
        os << v;
    } else if constexpr (is_streamable<T>::value) {
        os << v;
    } else if constexpr (is_iterable<T>::value) {
        os << "[";
        bool first = true;
        for (const auto& e : v) {
            if (!first) os << ", ";
            first = false;
            show(os, e);
        }
        os << "]";
    } else {
        os << "<" << sizeof(T) << "-byte value>";
    }
}

template <class A, class B>
[[noreturn]] void fail_eq(const char* file, int line, const char* ea, const char* eb, const A& a,
                          const B& b, const char* op) {
    std::ostringstream os;
    os << file << ":" << line << ": assertion `" << ea << " " << op << " " << eb << "` failed\n  left:  ";
    show(os, a);
    os << "\n  right: ";
    show(os, b);
    throw Failure{os.str()};
}

[[noreturn]] inline void fail(const char* file, int line, const std::string& what) {
    std::ostringstream os;
    os << file << ":" << line << ": " << what;
    throw Failure{os.str()};
}

}  // namespace s3test

#define S3_TEST_CAT_(a, b) a##b
#define S3_TEST_CAT(a, b) S3_TEST_CAT_(a, b)
#define S3_TEST(name)                                                                       \
    static void name();                                                                     \
    static ::s3test::Registrar S3_TEST_CAT(s3_test_registrar_, name)(__FILE__, #name, name); \
    static void name()

#define CHECK(cond)                                                                     \
    do {                                                                                \
        if (!(cond)) ::s3test::fail(__FILE__, __LINE__, "assertion failed: " #cond);   \
    } while (0)
#define CHECK_MSG(cond, msg)                                                                     \
    do {                                                                                         \
        if (!(cond)) {                                                                           \
            std::ostringstream s3_os_;                                                           \
            s3_os_ << "assertion failed: " #cond ": " << msg;                                    \
            ::s3test::fail(__FILE__, __LINE__, s3_os_.str());                                    \
        }                                                                                        \
    } while (0)
#define CHECK_EQ(a, b)                                                                     \
    do {                                                                                   \
        const auto& s3_a_ = (a);                                                           \
        const auto& s3_b_ = (b);                                                           \
        if (!(s3_a_ == s3_b_)) ::s3test::fail_eq(__FILE__, __LINE__, #a, #b, s3_a_, s3_b_, "=="); \
    } while (0)
#define CHECK_NE(a, b)                                                                     \
    do {                                                                                   \
        const auto& s3_a_ = (a);                                                           \
        const auto& s3_b_ = (b);                                                           \
        if (s3_a_ == s3_b_) ::s3test::fail_eq(__FILE__, __LINE__, #a, #b, s3_a_, s3_b_, "!="); \
    } while (0)
#define CHECK_BITS_EQ(a, b)                                                                     \
    do {                                                                                        \
        const float s3_a_ = (a);                                                                \
        const float s3_b_ = (b);                                                                \
        if (::s3::rs::to_bits(s3_a_) != ::s3::rs::to_bits(s3_b_))                               \
            ::s3test::fail_eq(__FILE__, __LINE__, #a, #b, s3_a_, s3_b_, "bits==");              \
    } while (0)
#define CHECK_PANICS(expr)                                                                  \
    do {                                                                                    \
        bool s3_panicked_ = false;                                                          \
        try {                                                                               \
            (void)(expr);                                                                   \
        } catch (const ::s3::rs::Panic&) {                                                  \
            s3_panicked_ = true;                                                            \
        }                                                                                   \
        if (!s3_panicked_) ::s3test::fail(__FILE__, __LINE__, "expected panic: " #expr);   \
    } while (0)
