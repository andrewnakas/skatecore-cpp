// Fork-B local helper: a minimal mutable slice (`&mut [T]`) view for solver records.
#pragma once
#include <array>
#include <cstddef>
#include <vector>

#include "s3/core/rs.hpp"

namespace s3::physics {

template <class T>
struct Slice {
    T* ptr = nullptr;
    std::size_t len = 0;

    Slice() = default;
    Slice(T* p, std::size_t n) : ptr(p), len(n) {}
    Slice(std::vector<T>& v) : ptr(v.data()), len(v.size()) {}
    template <std::size_t N>
    Slice(std::array<T, N>& a) : ptr(a.data()), len(N) {}

    std::size_t size() const { return len; }
    bool empty() const { return len == 0; }
    T* begin() const { return ptr; }
    T* end() const { return ptr + len; }
    T& operator[](std::size_t i) const {
        S3_ASSERT(i < len);
        return ptr[i];
    }
};

}  // namespace s3::physics
