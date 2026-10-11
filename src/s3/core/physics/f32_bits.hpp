// Fork-B local helper: constexpr `f32::from_bits` for `const` items (Rust `const X: f32 =
// f32::from_bits(..)`). rs::from_bits uses memcpy and is not constexpr in C++17.
#pragma once
#include <cstdint>

namespace s3::physics {

constexpr float f32_from_bits(std::uint32_t bits) { return __builtin_bit_cast(float, bits); }

}  // namespace s3::physics
