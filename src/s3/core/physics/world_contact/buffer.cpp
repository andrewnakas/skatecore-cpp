// Port of skate-core/src/physics/world_contact/buffer.rs.
#include "s3/core/physics/world_contact/buffer.hpp"

#include <cstring>

#include "s3/core/physics/world_contact/reduction.hpp"

namespace s3::physics::world_contact::buffer {

using arithmetic::dot;
using arithmetic::sub;
using arithmetic::vector;

std::optional<std::size_t> ContactBuffer::allocate(const ContactSink& sink) {
    S3_ASSERT(count <= 50);
    if (count + flushed >= capacity) {
        if (full == 0) {
            if (deferred_reduction != 0) {
                reduce();
            }
            if (count + flushed >= capacity) {
                full = 1;
            }
        }
        if (full != 0) {
            dropped = dropped + 1u;
            return std::nullopt;
        }
    }
    if (count >= 50) {
        if (allow_flush == 0) {
            full = 1;
            return std::nullopt;
        }
        flush(sink);
    }
    const std::size_t index = count;
    count += 1;
    return index;
}

bool ContactBuffer::last_is_duplicate() {
    if (distance_squared_threshold < 0.0f || deferred_reduction != 0) {
        return false;
    }
    S3_ASSERT(count > 0 && count <= 50);
    const ContactRecord& current = records[count - 1];
    double limit;
    const std::uint64_t limit_bits = 0x3fee147ae147ae14ull;
    std::memcpy(&limit, &limit_bits, sizeof limit);
    for (std::size_t p = 0; p + 1 < count; ++p) {
        const ContactRecord& previous = records[p];
        if (previous[3] != current[3] || previous[7] != current[7]) {
            continue;
        }
        const auto a = sub(vector(previous, 0), vector(current, 0));
        const auto b = sub(vector(previous, 4), vector(current, 4));
        if (distance_squared_threshold > dot(a, a) && dot(b, b) < distance_squared_threshold &&
            static_cast<double>(dot(vector(previous, 8), vector(current, 8))) > limit) {
            dropped = dropped + 1u;
            return true;
        }
    }
    return false;
}

void ContactBuffer::reduce() { reduction::reduce(records, count); }

void ContactBuffer::flush(const ContactSink& sink) {
    if (count == 0) {
        return;
    }
    if (deferred_reduction != 0) {
        reduce();
    }
    sink(records.data(), count);
    flushed = flushed + count;
    count = 0;
}

}  // namespace s3::physics::world_contact::buffer
