// Port of simref sim/src/camera/shake_data.rs.
#include "s3/game/camera/shake_data.hpp"

#include <array>
#include <cstdint>
#include <vector>

#include "s3/data/result.hpp"

namespace s3::game::camera::shake_data {
namespace {

/// `str::split_whitespace` (ASCII + Unicode White_Space; the stock files are ASCII).
std::vector<std::string> split_whitespace(const std::string& text) {
    std::vector<std::string> out;
    std::string current;
    for (char ch : text) {
        const bool space = ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\x0b' || ch == '\x0c';
        if (space) {
            if (!current.empty()) out.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) out.push_back(std::move(current));
    return out;
}

}  // namespace

rs::Result<s3::camera::ShakeSamples, std::string> parse(const std::string& text) {
    const std::vector<std::string> tokens = split_whitespace(text);
    std::size_t next = 0;
    if (tokens.empty()) {
        return rs::make_err(std::string("Empty camera .shk file"));
    }
    std::uint64_t count = 0;
    if (!s3::data::parse_u64_radix(tokens[next], 10, count)) {
        return rs::make_err("Invalid camera shake sample count: " + tokens[next]);
    }
    ++next;
    std::vector<std::array<float, 8>> rows;
    rows.reserve(std::size_t(count));
    for (std::uint64_t index = 0; index < count; ++index) {
        std::array<float, 8> row{};
        for (float& field : row) {
            if (next >= tokens.size()) {
                return rs::make_err("Truncated camera shake sample " + std::to_string(index));
            }
            if (!s3::data::parse_f32(tokens[next], field)) {
                return rs::make_err("Invalid camera shake sample " + std::to_string(index) + ": " + tokens[next]);
            }
            ++next;
        }
        rows.push_back(row);
    }
    if (next < tokens.size()) {
        return rs::make_err(std::string("Camera shake has extra sample data"));
    }
    return s3::camera::ShakeSamples::from_rows(rows);
}

}  // namespace s3::game::camera::shake_data
