// Port of skate-core/src/input/body_flip_signal.rs.
// BodyFlippingSignal82BA3380/33B8/35E0/3790. Both windows update on
// every call before deciding whether to create or remove the motion intent.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace s3::input::body_flip_signal {

struct Settings {
    /// anim_motion.extend_bodyflip_gesture and extend_takeoff_point.
    float gesture_window = 0.0f;
    float takeoff_window = 0.0f;
};

struct State {
    // Private in Rust.
    float gesture_time = 0.0f;
    float takeoff_time = 0.0f;
    /// Ordered native keys830C01B4 then830BE3A8. None is the constructor's
    /// null name. The host binds their recovered text to its intent storage.
    std::optional<std::size_t> selected;

    void begin(Settings settings) {
        gesture_time = settings.gesture_window;
        takeoff_time = settings.takeoff_window;
    }

    /// Some(index) sets only that intent to1. None removes BOTH keys.
    /// Incoming intentions are tested for presence, not their scalar value.
    std::optional<std::size_t> update(std::array<bool, 2> present, std::uint32_t filtered_category, float dt,
                                      Settings settings);
};

}  // namespace s3::input::body_flip_signal
