// Port of skate-core/src/animation/commands/buffers.rs.
// Typed animation allocations. Slots identify whole allocations, never addresses.
#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <variant>
#include <vector>

#include "s3/core/animation/output/native.hpp"
#include "s3/core/animation/output/sqt.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::commands::buffers {

using output::NativeMatrix;
using output::sqt::Sqt;

/// pub(crate): process-wide unique storage identity (panics when exhausted).
std::size_t new_owner();

template <class T>
struct BoneSlice;

template <class T>
struct Buffer {
    // Private in Rust; only BoneBuffers creates them.
    std::size_t slot = 0;
    std::size_t owner = 0;

    BoneSlice<T> at(std::size_t first_bone) const { return BoneSlice<T>{*this, first_bone}; }

    bool operator==(const Buffer& o) const { return slot == o.slot && owner == o.owner; }
    bool operator!=(const Buffer& o) const { return !(*this == o); }
};

template <class T>
struct BoneSlice {
    Buffer<T> buffer;
    std::size_t first_bone = 0;

    bool operator==(const BoneSlice& o) const { return buffer == o.buffer && first_bone == o.first_bone; }
    bool operator!=(const BoneSlice& o) const { return !(*this == o); }
};

/// Rust `enum BufferError`; each variant is a nested struct held in `value`.
struct BufferError {
    struct UnknownBuffer {};
    struct RangeOutsideAllocation {};
    struct UninitializedBone {
        std::size_t bone = 0;
    };
    /// The native copy helper's partially overlapping behavior is not recovered.
    struct UnsupportedPartialOverlap {};

    std::variant<UnknownBuffer, RangeOutsideAllocation, UninitializedBone, UnsupportedPartialOverlap> value;

    BufferError(UnknownBuffer v) : value(v) {}
    BufferError(RangeOutsideAllocation v) : value(v) {}
    BufferError(UninitializedBone v) : value(v) {}
    BufferError(UnsupportedPartialOverlap v) : value(v) {}
};
S3_DERIVE_EQ_EMPTY(BufferError::UnknownBuffer)
S3_DERIVE_EQ_EMPTY(BufferError::RangeOutsideAllocation)
S3_DERIVE_EQ(BufferError::UninitializedBone, bone)
S3_DERIVE_EQ_EMPTY(BufferError::UnsupportedPartialOverlap)
S3_DERIVE_EQ(BufferError, value)

/// `Range<usize>`.
struct BoneRange {
    std::size_t start = 0;
    std::size_t end = 0;
};

/// Move-only (Rust's BoneBuffers is not Clone: its owner identity is unique).
/// `BoneBuffers<T>{}` == Rust `Default` (allocates a fresh owner identity).
template <class T>
class BoneBuffers {
public:
    BoneBuffers() : owner_(new_owner()) {}
    static BoneBuffers default_() { return BoneBuffers(); }
    BoneBuffers(const BoneBuffers&) = delete;
    BoneBuffers& operator=(const BoneBuffers&) = delete;
    BoneBuffers(BoneBuffers&&) = default;
    BoneBuffers& operator=(BoneBuffers&&) = default;

    /// Allocate exactly the requested extent without inventing a bind pose.
    Buffer<T> allocate(std::size_t count) { return insert(std::vector<std::optional<T>>(count)); }
    template <class Range>
    Buffer<T> initialized(const Range& values) {
        std::vector<std::optional<T>> allocation;
        for (const auto& v : values) allocation.emplace_back(v);
        return insert(std::move(allocation));
    }
    Buffer<T> initialized(std::initializer_list<T> values) {
        std::vector<std::optional<T>> allocation;
        for (const auto& v : values) allocation.emplace_back(v);
        return insert(std::move(allocation));
    }

    rs::Result<std::vector<T>, BufferError> read(BoneSlice<T> source, std::size_t count) const {
        if (count == 0) return std::vector<T>{};
        auto range = this->range(source, count);
        if (range.is_err()) return rs::make_err(range.unwrap_err());
        const BoneRange r = range.unwrap();
        std::vector<T> out;
        out.reserve(count);
        for (std::size_t bone = r.start; bone < r.end; ++bone) {
            const auto& value = allocations_[source.buffer.slot][bone];
            if (!value) return rs::make_err(BufferError(BufferError::UninitializedBone{bone}));
            out.push_back(*value);
        }
        return out;
    }

    /// pub(crate) in Rust.
    rs::Result<rs::Unit, BufferError> copy(BoneSlice<T> source, BoneSlice<T> destination,
                                           std::size_t count) {
        if (count == 0) return rs::Unit{};
        auto from_r = range(source, count);
        if (from_r.is_err()) return rs::make_err(from_r.unwrap_err());
        auto to_r = range(destination, count);
        if (to_r.is_err()) return rs::make_err(to_r.unwrap_err());
        const BoneRange from = from_r.unwrap();
        const BoneRange to = to_r.unwrap();
        if (source.buffer.slot == destination.buffer.slot) {
            if (from.start == to.start && from.end == to.end) return rs::Unit{};
            if (from.start < to.end && to.start < from.end) {
                return rs::make_err(BufferError(BufferError::UnsupportedPartialOverlap{}));
            }
        }
        // Copy validity alongside the value: native SET does not initialize
        // unwritten bones. Such exports remain unreadable, rather than zero-filled.
        for (std::size_t i = 0; i < count; ++i) {
            allocations_[destination.buffer.slot][to.start + i] =
                allocations_[source.buffer.slot][from.start + i];
        }
        return rs::Unit{};
    }

    std::size_t owner() const { return owner_; }

private:
    Buffer<T> insert(std::vector<std::optional<T>> allocation) {
        const std::size_t slot = allocations_.size();
        allocations_.push_back(std::move(allocation));
        return Buffer<T>{slot, owner_};
    }
    rs::Result<BoneRange, BufferError> range(BoneSlice<T> source, std::size_t count) const {
        if (source.buffer.owner != owner_) {
            return rs::make_err(BufferError(BufferError::UnknownBuffer{}));
        }
        if (source.buffer.slot >= allocations_.size()) {
            return rs::make_err(BufferError(BufferError::UnknownBuffer{}));
        }
        const auto& allocation = allocations_[source.buffer.slot];
        if (source.first_bone > SIZE_MAX - count) {
            return rs::make_err(BufferError(BufferError::RangeOutsideAllocation{}));
        }
        const std::size_t end = source.first_bone + count;
        if (end > allocation.size()) {
            return rs::make_err(BufferError(BufferError::RangeOutsideAllocation{}));
        }
        return BoneRange{source.first_bone, end};
    }

    std::size_t owner_;
    std::vector<std::vector<std::optional<T>>> allocations_;
};

/// `PoseBuffers{}` == Rust `Default`. Move-only.
struct PoseBuffers {
    BoneBuffers<Sqt> sqt;
    BoneBuffers<NativeMatrix> matrices;
};

}  // namespace s3::animation::commands::buffers
