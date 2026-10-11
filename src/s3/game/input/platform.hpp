// Port of skate-game/src/input/platform.rs.
//! Windows device transport. Raw signed axes/trigger bytes reach the TU3
//! converter without Bevy/gilrs deadzones or normalized-axis reconstruction.
//
// C++ host: neither the `#[cfg(windows)]` XInput backend nor the
// `#[cfg(target_arch = "wasm32")]` Bevy/gilrs web backend is ported; `poll_cached`
// takes the `#[cfg(not(any(windows, target_arch = "wasm32")))]` path and returns
// `DeviceError::UnsupportedPlatform`. `skate_vfs::time::Instant`/`Duration` ->
// `std::chrono::steady_clock::time_point`/`duration`.
#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>

#include "s3/core/input/xbox.hpp"
#include "s3/core/rs.hpp"

namespace s3::game::input::platform {

using Instant = std::chrono::steady_clock::time_point;
using Duration = std::chrono::steady_clock::duration;

struct DevicePacket {
    std::uint32_t number = 0;
    ::s3::input::xbox::XboxState state;
    std::uint8_t subtype = 0;
};

/// `#[derive(Clone, Copy, Debug, PartialEq, Eq)] enum DeviceError`.
struct DeviceError {
    struct Disconnected {};
    struct State {
        std::uint32_t _0 = 0;
    };
    struct Capabilities {
        std::uint32_t _0 = 0;
    };
    /// `#[cfg(not(windows))]`.
    struct UnsupportedPlatform {};
    std::variant<Disconnected, State, Capabilities, UnsupportedPlatform> value;

    static DeviceError disconnected() { return DeviceError{Disconnected{}}; }
    static DeviceError state(std::uint32_t code) { return DeviceError{State{code}}; }
    static DeviceError capabilities(std::uint32_t code) { return DeviceError{Capabilities{code}}; }
    static DeviceError unsupported_platform() { return DeviceError{UnsupportedPlatform{}}; }
};
inline bool operator==(const DeviceError& a, const DeviceError& b) {
    if (a.value.index() != b.value.index()) return false;
    if (auto* x = std::get_if<DeviceError::State>(&a.value)) return x->_0 == std::get<DeviceError::State>(b.value)._0;
    if (auto* x = std::get_if<DeviceError::Capabilities>(&a.value))
        return x->_0 == std::get<DeviceError::Capabilities>(b.value)._0;
    return true;
}
inline bool operator!=(const DeviceError& a, const DeviceError& b) { return !(a == b); }

using DeviceResult = rs::Result<DevicePacket, DeviceError>;

/// Device identity is metadata; raw input is still sampled every host frame.
/// Refresh periodically as well as after errors, so hot swaps cannot leave a
/// subtype cached indefinitely even if Windows never exposes a disconnect.
/// `#[derive(Default)]`.
struct CapabilityCache {
    // Private in Rust.
    std::optional<std::pair<std::uint8_t, Instant>> value;

    void invalidate() { value.reset(); }
    /// Private in Rust (`fn get(&mut self, now, read: impl FnOnce() -> Result<u8, DeviceError>)`).
    template <class F>
    rs::Result<std::uint8_t, DeviceError> get(Instant now, F&& read) {
        if (value) {
            const auto [subtype, expires] = *value;
            if (now < expires) return subtype;
        }
        value.reset();
        rs::Result<std::uint8_t, DeviceError> result = read();
        if (result.is_err()) return result;
        const std::uint8_t subtype = result.unwrap();
        value = std::make_pair(subtype, now + std::chrono::duration_cast<Duration>(std::chrono::seconds(1)));
        return subtype;
    }
};

DeviceResult poll_cached(std::size_t index, CapabilityCache& cache);

// Preserve the uncached API for menu-only polling.
DeviceResult poll(std::size_t index);

}  // namespace s3::game::input::platform
