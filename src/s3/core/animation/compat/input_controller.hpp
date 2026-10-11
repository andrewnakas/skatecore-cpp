// Compat for skate-core/src/input/controller.rs `ActionMap` (owned by the input stage).
// FALLBACK OWNED BY STAGE A: delete once s3/core/input/controller.hpp exists.
#pragma once
#if __has_include("s3/core/input/controller.hpp")
#include "s3/core/input/controller.hpp"
#else
#include <cstdint>

namespace s3::input::controller {

/// Rust `trait ActionMap`: evaluated native cInputMap actions (call order matters).
struct ActionMap {
    virtual ~ActionMap() = default;
    virtual float value(std::uint32_t action) = 0;
    virtual std::uint8_t state(std::uint32_t action) = 0;
};

}  // namespace s3::input::controller
#endif
