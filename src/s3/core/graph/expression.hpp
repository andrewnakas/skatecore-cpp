// Port of skate-core/src/graph/expression.rs.
//! TU3 ExpressionOperator::GetActivationMasked (82C12508).
//! Excluded children do not participate in AND/OR/NOT. Preserve lazy calls:
//! condition operations may read or modify their own instance state.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "s3/core/rs.hpp"

namespace s3::graph::expression {

/// ParseOperator 82C12438: case-sensitive strings, zero for missing/unknown.
inline std::uint32_t parse_operator(std::optional<std::string_view> name) {
    if (!name) return 0;
    if (*name == "and") return 1;
    if (*name == "or") return 2;
    if (*name == "not") return 3;
    return 0;
}

/// Execute the complete operator with the native virtual child-call boundary.
/// The callback receives its ordered child index and the initially true
/// excluded byte; it returns the native result word. Context/mask are supplied
/// by the caller's closure, without an invented default condition result.
/// NOT requires a first child, as in the native caller contract.
/// `child` is `FnMut(std::size_t index, std::uint8_t& excluded) -> std::uint32_t`.
template <class F>
std::uint32_t evaluate_operator(std::uint32_t operator_, std::size_t child_count,
                                std::uint8_t& excluded, F&& child) {
    if (operator_ > 3) {
        return 1;  // Native default leaves the caller's excluded byte untouched.
    }
    if (operator_ == 3) {
        if (!(child_count > 0)) S3_PANIC("stock NOT expression requires a child");
        std::uint8_t child_excluded = 1;
        const std::uint8_t value = static_cast<std::uint8_t>(child(std::size_t(0), child_excluded));
        excluded = child_excluded;
        return child_excluded != 0 ? 1u : std::uint32_t(value == 0);
    }
    const bool is_or = operator_ == 2;
    std::uint32_t value = is_or ? 0u : 1u;
    std::uint8_t all_excluded = 1;
    for (std::size_t index = 0; index < child_count; ++index) {
        if ((is_or && static_cast<std::uint8_t>(value) != 0) ||
            (!is_or && static_cast<std::uint8_t>(value) == 0)) {
            break;
        }
        std::uint8_t child_excluded = 1;
        const std::uint32_t result = child(index, child_excluded);
        if (child_excluded == 0) {
            all_excluded = 0;
            value = is_or ? result
                          : std::uint32_t(static_cast<std::uint8_t>(static_cast<std::uint8_t>(result) &
                                                                    static_cast<std::uint8_t>(value)));
        }
    }
    excluded = all_excluded;
    return all_excluded != 0 ? 1u : value;
}

}  // namespace s3::graph::expression
