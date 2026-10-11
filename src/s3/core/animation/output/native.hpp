// Port of skate-core/src/animation/output/mod.rs (shared item types).
// `NativeMatrix` and `PoseBufferError` live here so sqt/hierarchy can include them without
// a cycle through output.hpp; output.hpp re-exports everything under s3::animation::output.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>

#include "s3/core/rs.hpp"

namespace s3::animation::output {

/// Four consecutive native vectors, including all packed fourth lanes.
/// Row/column conversion for a renderer belongs outside this module.
using NativeMatrix = std::array<std::array<float, 4>, 4>;

/// Rust `enum PoseBufferError`; each variant is a nested struct held in `value`.
struct PoseBufferError {
    struct NonFiniteWeight {};
    struct InvalidQuaternion {};
    struct CountOverflow {};
    struct ShortInput {};
    struct ShortOutput {};
    struct ShortParents {};
    struct InvalidCacheOffset {};
    struct InvalidParent {
        std::size_t bone = 0;
        std::int32_t parent = 0;
    };

    std::variant<NonFiniteWeight, InvalidQuaternion, CountOverflow, ShortInput, ShortOutput,
                 ShortParents, InvalidCacheOffset, InvalidParent>
        value;

    PoseBufferError(NonFiniteWeight v) : value(v) {}
    PoseBufferError(InvalidQuaternion v) : value(v) {}
    PoseBufferError(CountOverflow v) : value(v) {}
    PoseBufferError(ShortInput v) : value(v) {}
    PoseBufferError(ShortOutput v) : value(v) {}
    PoseBufferError(ShortParents v) : value(v) {}
    PoseBufferError(InvalidCacheOffset v) : value(v) {}
    PoseBufferError(InvalidParent v) : value(v) {}
};
S3_DERIVE_EQ_EMPTY(PoseBufferError::NonFiniteWeight)
S3_DERIVE_EQ_EMPTY(PoseBufferError::InvalidQuaternion)
S3_DERIVE_EQ_EMPTY(PoseBufferError::CountOverflow)
S3_DERIVE_EQ_EMPTY(PoseBufferError::ShortInput)
S3_DERIVE_EQ_EMPTY(PoseBufferError::ShortOutput)
S3_DERIVE_EQ_EMPTY(PoseBufferError::ShortParents)
S3_DERIVE_EQ_EMPTY(PoseBufferError::InvalidCacheOffset)
S3_DERIVE_EQ(PoseBufferError::InvalidParent, bone, parent)
S3_DERIVE_EQ(PoseBufferError, value)

}  // namespace s3::animation::output
