// Rust `Box<T>` for the animation port: an owning, never-null, deep-copying value wrapper.
// Needed by recursive Clone-able enums (PlaybackTree / PlaybackTransition). Not a Rust module.
#pragma once
#include <memory>
#include <utility>

namespace s3::animation {

template <class T>
class Box {
public:
    Box(const T& v) : p_(std::make_unique<T>(v)) {}
    Box(T&& v) : p_(std::make_unique<T>(std::move(v))) {}
    Box(const Box& o) : p_(std::make_unique<T>(*o.p_)) {}
    Box(Box&& o) noexcept : p_(std::move(o.p_)) {}
    Box& operator=(const Box& o) {
        if (this != &o) p_ = std::make_unique<T>(*o.p_);
        return *this;
    }
    Box& operator=(Box&& o) noexcept {
        p_ = std::move(o.p_);
        return *this;
    }
    ~Box() = default;

    T& operator*() { return *p_; }
    const T& operator*() const { return *p_; }
    T* operator->() { return p_.get(); }
    const T* operator->() const { return p_.get(); }
    T* get() { return p_.get(); }
    const T* get() const { return p_.get(); }

private:
    std::unique_ptr<T> p_;
};

}  // namespace s3::animation
