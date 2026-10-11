// Port of skate-core/src/input/body_flip_signal.rs.
// BodyFlippingSignal82BA3380/33B8/35E0/3790.
#include "s3/core/input/body_flip_signal.hpp"

namespace s3::input::body_flip_signal {

namespace {
float cap(float value, float limit) { return value - limit >= 0.0f ? limit : value; }
}  // namespace

std::optional<std::size_t> State::update(std::array<bool, 2> present, std::uint32_t filtered_category, float dt,
                                         Settings settings) {
    std::optional<std::size_t> index;
    for (std::size_t i = 0; i < present.size(); ++i) {
        if (present[i]) {
            index = i;
            break;
        }
    }
    if (index) {
        gesture_time = 0.0f;
        selected = index;
    } else {
        gesture_time = cap(gesture_time + dt, settings.gesture_window);
    }
    const bool gesture = gesture_time < settings.gesture_window;
    takeoff_time = filtered_category == 2 ? cap(takeoff_time + dt, settings.takeoff_window) : 0.0f;
    const bool takeoff = !(takeoff_time <= 0.0f) && takeoff_time < settings.takeoff_window;
    if (gesture && takeoff) {
        return selected;
    }
    return std::nullopt;
}

}  // namespace s3::input::body_flip_signal
