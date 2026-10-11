// Port of skate-core/src/animation/output/sqt.rs.
#include "s3/core/animation/output/sqt.hpp"

namespace s3::animation::output::sqt {

rs::Result<std::size_t, PoseBufferError> sqt_to_local(std::size_t bone_count, std::uint8_t cache_offset,
                                                      Slice<const Sqt> source,
                                                      Slice<NativeMatrix> destination) {
    if (cache_offset >= 128 || cache_offset % 16 != 0) {
        return rs::make_err(PoseBufferError(PoseBufferError::InvalidCacheOffset{}));
    }
    if (bone_count > SIZE_MAX - 3) {
        return rs::make_err(PoseBufferError(PoseBufferError::CountOverflow{}));
    }
    const std::size_t padded = (bone_count + 3) & ~std::size_t(3);
    if (source.size() < padded) {
        return rs::make_err(PoseBufferError(PoseBufferError::ShortInput{}));
    }
    const std::size_t trailing_words =
        padded == 0 ? 0 : (128 - static_cast<std::size_t>(cache_offset)) / 4;
    const std::size_t lines = (trailing_words + 15) / 16;
    if (padded > SIZE_MAX - lines) {
        return rs::make_err(PoseBufferError(PoseBufferError::CountOverflow{}));
    }
    const std::size_t required = padded + lines;
    if (destination.size() < required) {
        return rs::make_err(PoseBufferError(PoseBufferError::ShortOutput{}));
    }
    // Earlier zeroed lines are overwritten by later groups; only the final tail
    // remains visible. No typed input/output aliasing is allowed by this adapter.
    std::size_t taken = 0;
    for (std::size_t m = padded; m < required && taken < trailing_words; ++m) {
        for (std::size_t r = 0; r < 4 && taken < trailing_words; ++r) {
            for (std::size_t l = 0; l < 4 && taken < trailing_words; ++l) {
                destination[m][r][l] = 0.0f;
                ++taken;
            }
        }
    }
    for (std::size_t i = 0; i < padded; ++i) {
        destination[i] = sqt_to_matrix(source[i]);
    }
    return padded;
}

NativeMatrix sqt_to_matrix(Sqt input) {
    const float x = input.rotation[0];
    const float y = input.rotation[1];
    const float z = input.rotation[2];
    const float w = input.rotation[3];
    const float sx = input.scale[0];
    const float sy = input.scale[1];
    const float sz = input.scale[2];
    // Separate products and sums reflect the native VMX instruction order.
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float wz = w * z;
    const float xz = x * z;
    const float wy = w * y;
    const float yz = y * z;
    const float wx = w * x;
    return NativeMatrix{{
        {
            rs::mul_add(-(yy + zz), 2.0f, 1.0f) * sx,
            ((xy + wz) * 2.0f) * sx,
            ((xz - wy) * 2.0f) * sx,
            0.0f,
        },
        {
            ((xy - wz) * 2.0f) * sy,
            rs::mul_add(-(xx + zz), 2.0f, 1.0f) * sy,
            ((yz + wx) * 2.0f) * sy,
            0.0f,
        },
        {
            ((xz + wy) * 2.0f) * sz,
            ((yz - wx) * 2.0f) * sz,
            rs::mul_add(-(xx + yy), 2.0f, 1.0f) * sz,
            0.0f,
        },
        {
            input.translation[0],
            input.translation[1],
            input.translation[2],
            1.0f,
        },
    }};
}

}  // namespace s3::animation::output::sqt
