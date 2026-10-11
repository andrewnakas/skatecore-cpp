// Port of skate-core/src/animation/output/attributes.rs.
// PARTIAL PORT (stage C): only items used by physics (AttributeName). Extend in place.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::animation::output::attributes {

/// Rust tuple struct `AttributeName(pub [u32; 5])`; `_0` is Rust's `.0`.
struct AttributeName {
    std::array<std::uint32_t, 5> _0{};
};
S3_DERIVE_EQ(AttributeName, _0)

}  // namespace s3::animation::output::attributes
