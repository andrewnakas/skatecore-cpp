// Rust slice `&[T]` / `&mut [T]` for the animation/graph port: `Slice<const T>` / `Slice<T>`.
// Non-owning (pointer, length); implicitly constructible from std::vector, std::array,
// a C array, or (pointer, length). Bounds-checked operator[] panics like Rust indexing.
#pragma once
#include <array>
#include <cstddef>
#include <type_traits>
#include <vector>

#include "s3/core/rs.hpp"

namespace s3::animation {

template <class T>
class Slice {
public:
    using value_type = std::remove_cv_t<T>;
    constexpr Slice() = default;
    constexpr Slice(T* data, std::size_t len) : data_(data), len_(len) {}
    template <class U, class = std::enable_if_t<std::is_convertible_v<U (*)[], T (*)[]>>>
    Slice(std::vector<U>& v) : data_(v.data()), len_(v.size()) {}
    template <class U, class = std::enable_if_t<std::is_convertible_v<const U (*)[], T (*)[]>>>
    Slice(const std::vector<U>& v) : data_(v.data()), len_(v.size()) {}
    template <class U, std::size_t N, class = std::enable_if_t<std::is_convertible_v<U (*)[], T (*)[]>>>
    constexpr Slice(std::array<U, N>& a) : data_(a.data()), len_(N) {}
    template <class U, std::size_t N,
              class = std::enable_if_t<std::is_convertible_v<const U (*)[], T (*)[]>>>
    constexpr Slice(const std::array<U, N>& a) : data_(a.data()), len_(N) {}
    template <std::size_t N>
    constexpr Slice(T (&a)[N]) : data_(a), len_(N) {}
    template <class U, class = std::enable_if_t<std::is_convertible_v<U (*)[], T (*)[]>>>
    constexpr Slice(Slice<U> s) : data_(s.data()), len_(s.size()) {}

    constexpr T* data() const { return data_; }
    constexpr std::size_t size() const { return len_; }
    constexpr std::size_t len() const { return len_; }
    constexpr bool empty() const { return len_ == 0; }
    constexpr bool is_empty() const { return len_ == 0; }
    constexpr T* begin() const { return data_; }
    constexpr T* end() const { return data_ + len_; }
    T& operator[](std::size_t i) const {
        if (i >= len_) S3_PANIC("index out of bounds");
        return data_[i];
    }
    /// `slice.get(i)` -> nullptr when out of range.
    T* get(std::size_t i) const { return i < len_ ? data_ + i : nullptr; }
    /// `&slice[a..b]`.
    Slice sub(std::size_t a, std::size_t b) const {
        if (a > b || b > len_) S3_PANIC("slice index out of range");
        return Slice(data_ + a, b - a);
    }
    T* first() const { return len_ ? data_ : nullptr; }
    T* last() const { return len_ ? data_ + len_ - 1 : nullptr; }
    std::vector<value_type> to_vec() const { return std::vector<value_type>(begin(), end()); }

private:
    T* data_ = nullptr;
    std::size_t len_ = 0;
};

template <class T>
inline bool operator==(Slice<T> a, Slice<T> b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!(a.data()[i] == b.data()[i])) return false;
    return true;
}

}  // namespace s3::animation
