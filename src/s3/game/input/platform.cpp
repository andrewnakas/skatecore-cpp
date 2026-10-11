// Port of skate-game/src/input/platform.rs.
#include "s3/game/input/platform.hpp"

namespace s3::game::input::platform {

DeviceResult poll_cached(std::size_t index, CapabilityCache& cache) {
    S3_ASSERT(index < 4);
    (void)cache;
    // #[cfg(not(any(windows, target_arch = "wasm32")))]
    return rs::make_err(DeviceError::unsupported_platform());
}

DeviceResult poll(std::size_t index) {
    CapabilityCache cache;
    return poll_cached(index, cache);
}

}  // namespace s3::game::input::platform
