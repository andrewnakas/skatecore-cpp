// Shared C++-only helpers for the motion_* graph_host ports (NOT a Rust module).
// Rust's `data.float(..)?`, `data.words::<N>(..)?` and `PointGraph { x: from_fn(..), y: .. }`
// patterns, plus `?` on `rs::Result<_, std::string>`.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"

namespace s3::game::graph_host::motion_collections {

/// `data.float(cls, key, name)` as `rs::Result<f32, String>`.
inline rs::Result<float, std::string> float_(const s3::data::Collections& data, const char* cls, const char* key,
                                             const char* name) {
    auto r = data.float_(cls, key, name);
    if (!r.ok) return rs::make_err(r.error);
    return r.value;
}

/// `data.words::<N>(cls, key, name)` as `rs::Result<[u32; N], String>`.
template <std::size_t N>
rs::Result<std::array<std::uint32_t, N>, std::string> words(const s3::data::Collections& data, const char* cls,
                                                            const char* key, const char* name) {
    auto r = data.words<N>(cls, key, name);
    if (!r.ok) return rs::make_err(r.error);
    return r.value;
}

/// `PointGraph { x: from_fn(|i| from_bits(w[x0 + i])), y: from_fn(|i| from_bits(w[y0 + i])) }`.
template <std::size_t P, std::size_t N>
s3::point_graph::PointGraph<P> graph(const std::array<std::uint32_t, N>& w, std::size_t x0, std::size_t y0) {
    s3::point_graph::PointGraph<P> out;
    out.x = rs::from_fn<P>([&](std::size_t i) { return rs::from_bits(w[x0 + i]); });
    out.y = rs::from_fn<P>([&](std::size_t i) { return rs::from_bits(w[y0 + i]); });
    return out;
}

}  // namespace s3::game::graph_host::motion_collections

/// Rust `let decl = expr?;` for `rs::Result<T, E>` inside a function returning `rs::Result<_, E>`.
#define S3_MOTION_CAT_(a, b) a##b
#define S3_MOTION_CAT(a, b) S3_MOTION_CAT_(a, b)
#define S3_MOTION_TRY_IMPL(tmp, decl, expr)                          \
    auto tmp = (expr);                                               \
    if (tmp.is_err()) return ::s3::rs::make_err(tmp.unwrap_err());   \
    decl = std::move(tmp.unwrap())
#define S3_MOTION_TRY(decl, expr) S3_MOTION_TRY_IMPL(S3_MOTION_CAT(s3_motion_try_, __LINE__), decl, expr)
/// Rust `expr?;` discarding the Ok value.
#define S3_MOTION_CHECK(expr)                                                                  \
    do {                                                                                       \
        auto s3_motion_check_ = (expr);                                                        \
        if (s3_motion_check_.is_err()) return ::s3::rs::make_err(s3_motion_check_.unwrap_err()); \
    } while (0)
