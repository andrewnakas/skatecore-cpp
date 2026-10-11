// Port of skate-core/src/animation/commands/buffers.rs.
#include "s3/core/animation/commands/buffers.hpp"

#include <atomic>

namespace s3::animation::commands::buffers {

std::size_t new_owner() {
    static std::atomic<std::size_t> next_owner{0};
    std::size_t value = next_owner.load(std::memory_order_relaxed);
    while (true) {
        if (value == SIZE_MAX) S3_PANIC("animation storage identity exhausted");
        if (next_owner.compare_exchange_weak(value, value + 1, std::memory_order_relaxed)) {
            return value;
        }
    }
}

}  // namespace s3::animation::commands::buffers
