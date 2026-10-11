// Shared lane aliases for the skate-core camera port (`[f32; 4]`, `[[f32; 4]; 4]`) and a
// Rust `?` helper for `rs::Result<T, String>`. Not a Rust module.
#pragma once
#include <array>
#include <string>

#include "s3/core/rs.hpp"

namespace s3::camera {

using F4 = std::array<float, 4>;
using F4x4 = std::array<F4, 4>;
using Error = std::string;

}  // namespace s3::camera

/// `let decl = expr?;` for `rs::Result<T, std::string>` inside a function returning
/// `rs::Result<U, std::string>`.
#define S3_CAM_CAT_(a, b) a##b
#define S3_CAM_CAT(a, b) S3_CAM_CAT_(a, b)
#define S3_CAM_TRY_IMPL(tmp, decl, expr)                                 \
    auto tmp = (expr);                                                   \
    if (tmp.is_err()) return ::s3::rs::make_err(std::string(tmp.unwrap_err())); \
    decl = std::move(tmp.unwrap())
#define S3_CAM_TRY(decl, expr) S3_CAM_TRY_IMPL(S3_CAM_CAT(s3_cam_try_, __LINE__), decl, expr)
