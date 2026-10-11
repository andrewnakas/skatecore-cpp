// Helpers shared by the skate-game camera port: s3::data::Result -> rs::Result<T, String>.
// Not a Rust module.
#pragma once
#include <string>
#include <utility>

#include "s3/core/camera/_types.hpp"
#include "s3/data/result.hpp"

namespace s3::game::camera {

template <class T>
rs::Result<T, std::string> from_data(s3::data::Result<T> r) {
    if (!r.ok) {
        return rs::make_err(std::move(r.error));
    }
    return std::move(r.value);
}

}  // namespace s3::game::camera

/// `let decl = data_expr?;` for an s3::data::Result inside a function returning rs::Result<_, String>.
#define S3_CAM_TRY_DATA(decl, expr) S3_CAM_TRY(decl, ::s3::game::camera::from_data(expr))
