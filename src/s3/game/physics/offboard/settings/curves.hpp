// Port of skate-game/src/physics/offboard/settings/curves.rs
// (also mounted as physics::offboard::air_selector::settings::curves via #[path]).
//! PointNegGraphData has four bounds followed by N X values and N Y values.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::offboard::settings::curves {

using sc::point_graph::PointGraph;

namespace detail {
/// Rust `u32::from_str_radix(s, 16)` over an 8-character slice.
inline rs::Result<std::uint32_t, std::string> u32_from_hex(const std::string& s) {
    std::size_t i = 0;
    if (!s.empty() && s[0] == '+') i = 1;
    if (i >= s.size()) return rs::make_err(std::string("invalid digit found in string"));
    std::uint64_t v = 0;
    for (; i < s.size(); ++i) {
        const char c = s[i];
        std::uint32_t d = 0;
        if (c >= '0' && c <= '9') {
            d = std::uint32_t(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = std::uint32_t(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            d = std::uint32_t(c - 'A' + 10);
        } else {
            return rs::make_err(std::string("invalid digit found in string"));
        }
        v = v * 16u + d;
        if (v > 0xffffffffull) return rs::make_err(std::string("number too large to fit in target type"));
    }
    return std::uint32_t(v);
}
inline bool is_whitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
}  // namespace detail

template <std::size_t N>
rs::Result<std::pair<PointGraph<N>, std::array<float, 4>>, std::string> parse(const std::string& text) {
    std::string hex;
    bool ascii = true;
    for (char c : text) {
        if (detail::is_whitespace(c)) continue;
        if (static_cast<unsigned char>(c) >= 0x80) ascii = false;
        hex.push_back(c);
    }
    if (N == 0 || !ascii || hex.size() != (4 + 2 * N) * 8) {
        return rs::make_err(std::string("Invalid native graph payload length"));
    }
    std::vector<float> values;
    values.reserve(4 + 2 * N);
    for (std::size_t i = 0; i < 4 + 2 * N; ++i) {
        auto word = detail::u32_from_hex(hex.substr(i * 8, 8));
        if (word.is_err()) return rs::make_err(word.unwrap_err());
        values.push_back(rs::from_bits(word.unwrap()));
    }
    for (float v : values) {
        if (!rs::is_finite(v)) return rs::make_err(std::string("Non-finite native graph value"));
    }
    PointGraph<N> graph{};
    for (std::size_t i = 0; i < N; ++i) {
        graph.x[i] = values[4 + i];
        graph.y[i] = values[4 + N + i];
    }
    // Duplicate authored X knots are valid and retained in source order.
    for (std::size_t i = 0; i + 1 < N; ++i) {
        if (graph.x[i] > graph.x[i + 1]) {
            return rs::make_err(std::string("Native graph X values are not ordered"));
        }
    }
    return std::make_pair(graph, std::array<float, 4>{values[0], values[1], values[2], values[3]});
}

template <std::size_t N>
rs::Result<std::pair<PointGraph<N>, std::array<float, 4>>, std::string> load(const sd::Collections& data,
                                                                          const std::string& class_,
                                                                          const std::string& name) {
    S3G_TRY(const sd::Field* field, data.field(class_, "default", name));
    if (field->type_name != fmt::cat("Sk8::PointNegGraphData", N)) {
        return rs::make_err(fmt::cat("Expected PointNegGraphData", N, " at ", class_, "/default/", name));
    }
    auto parsed = parse<N>(field->data);
    if (parsed.is_err()) return rs::make_err(fmt::cat(class_, "/default/", name, ": ", parsed.unwrap_err()));
    return parsed.unwrap();
}

}  // namespace s3::game::physics::offboard::settings::curves
