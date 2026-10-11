// Port of skate-core/src/animation/output/sqt.rs.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/animation/output/native.hpp"
#include "s3/core/animation/slice.hpp"

namespace s3::animation::output::sqt {

/// TU3 48-byte SQT record: scale, quaternion (x,y,z,w), translation.
struct Sqt {
    std::array<float, 4> scale{};
    std::array<float, 4> rotation{};
    std::array<float, 4> translation{};
};
S3_DERIVE_EQ(Sqt, scale, rotation, translation)

/// Complete arithmetic/store behavior of TU3 0x828D3800.
///
/// The native SIMD loop writes four records at a time, including padding beyond
/// `bone_count`. The native `dcbzl` additionally zeros the following cache line;
/// `cache_offset` specifies the destination address modulo 128 (a multiple of 16).
/// Destination must include that trailing storage. No normalization or
/// bind-pose composition occurs. Unlike native speculative loads, this safe
/// adapter does not read a further unused group beyond the padded inputs.
rs::Result<std::size_t, PoseBufferError> sqt_to_local(std::size_t bone_count, std::uint8_t cache_offset,
                                                      Slice<const Sqt> source,
                                                      Slice<NativeMatrix> destination);

/// Per-bone numerical result828D3800. The game owns its vector allocations;
/// cache-line padding and speculative DMA loads are not gameplay behavior.
NativeMatrix sqt_to_matrix(Sqt input);

}  // namespace s3::animation::output::sqt

namespace s3::animation::output {
// pub use sqt::{Sqt, sqt_to_local, sqt_to_matrix}; (repeated in output.hpp)
using sqt::Sqt;
using sqt::sqt_to_local;
using sqt::sqt_to_matrix;
}  // namespace s3::animation::output
