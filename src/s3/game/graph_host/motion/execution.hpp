// Port of skate-game/src/graph_host/motion_execute.rs (module `motion::execution`).
// The Rust file is an `impl MotionHost` block: `MotionHost::execute` is declared in
// s3/game/graph_host/motion.hpp and defined in motion/execution.cpp.
//
// This header also carries the small helpers shared by the motion/*_execution.cpp sources
// (Rust `?` on `Result<_, String>` and `{:?}` of the operation types used in error text).
#pragma once
#include <optional>
#include <string>
#include <utility>

#include "s3/core/rs.hpp"
#include "s3/game/graph_host/motion.hpp"

namespace s3::game::graph_host::motion::execution {

/// Rust `{:?}` of a `MotionOperation` (used only in diagnostic text).
std::string debug_operation(const MotionOperation& operation);
/// Rust `{:?}` of a stock gameplay `Operation` (used only in diagnostic text).
std::string debug_stock_gameplay(const motion_stock_gameplay::Operation& operation);

}  // namespace s3::game::graph_host::motion::execution

// `let x = expr?;` for an `rs::Result<T, std::string>` expression inside a function that returns
// `rs::Result<_, std::string>`.
#define S3_MHOST_CAT_(a, b) a##b
#define S3_MHOST_CAT(a, b) S3_MHOST_CAT_(a, b)
#define S3_MHOST_TRY_IMPL(tmp, decl, expr)                                  \
    auto tmp = (expr);                                                       \
    if (tmp.is_err()) return ::s3::rs::make_err(std::string(tmp.unwrap_err())); \
    decl = std::move(tmp.unwrap())
#define S3_MHOST_TRY(decl, expr) S3_MHOST_TRY_IMPL(S3_MHOST_CAT(s3_mhost_try_, __LINE__), decl, expr)
// `expr?;` discarding the value.
#define S3_MHOST_CHECK(expr)                                                              \
    do {                                                                                   \
        auto s3_mhost_check_ = (expr);                                                    \
        if (s3_mhost_check_.is_err())                                                     \
            return ::s3::rs::make_err(std::string(s3_mhost_check_.unwrap_err()));         \
    } while (0)
// `let x = option.ok_or("message")?;` (copies the value).
#define S3_MHOST_SOME(decl, option, message)                                         \
    if (!(option)) return ::s3::rs::make_err(std::string(message));                  \
    decl = *(option)
