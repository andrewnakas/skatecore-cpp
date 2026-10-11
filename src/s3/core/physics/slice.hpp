// Fork-B local helper: a minimal mutable slice (`&mut [T]`) view for solver records.
#pragma once
#include <array>
#include <cstddef>
#include <type_traits>
#include <vector>

#include "s3/core/rs.hpp"

namespace s3::physics {

template <class T>
struct Slice {
    T* ptr = nullptr;
    std::size_t len = 0;

    Slice() = default;
    Slice(T* p, std::size_t n) : ptr(p), len(n) {}
    template <class U, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    Slice(std::vector<U>& v) : ptr(v.data()), len(v.size()) {}
    template <class U, class = std::enable_if_t<std::is_convertible_v<const U*, T*>>>
    Slice(const std::vector<U>& v) : ptr(v.data()), len(v.size()) {}
    template <class U, std::size_t N, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    Slice(std::array<U, N>& a) : ptr(a.data()), len(N) {}
    template <class U, std::size_t N, class = std::enable_if_t<std::is_convertible_v<const U*, T*>>>
    Slice(const std::array<U, N>& a) : ptr(a.data()), len(N) {}

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
