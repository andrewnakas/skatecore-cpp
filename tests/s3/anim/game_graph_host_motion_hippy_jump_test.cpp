// Port of the #[cfg(test)] mod tests in skate-game/src/graph_host/motion_hippy_jump.rs.
#include <cmath>

#include "harness.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/motion_hippy_jump.hpp"

using namespace s3::game::graph_host::motion_hippy_jump;
using s3::animation::skeleton_input::name::encode;

namespace {
MotionAnimation animation() {
    auto metadata = s3::data::AnimationMetadata::parse(
        R"({"version":1,"source_bank":"test-only","source_sha256":"0000000000000000000000000000000000000000000000000000000000000000","source_bytes":48,"clips":[],"unsupported_trees":[]})");
    if (!metadata.ok) s3test::fail(__FILE__, __LINE__, metadata.error);
    return MotionAnimation::from_metadata(metadata.value);
}
}  // namespace

S3_TEST(tu3_update_uses_fixed_tick_and_exact_attribute_case) {
    Settings settings;
    settings.antic_length_to_hippy_height.x = s3::rs::from_fn<8>([](std::size_t i) { return static_cast<float>(i); });
    settings.antic_length_to_hippy_height.y =
        s3::rs::from_fn<8>([](std::size_t i) { return static_cast<float>(i) * 2.0f; });
    State state;
    MotionAnimation a = animation();

    state.begin();
    state.update(a, settings);

    CHECK_EQ(s3::rs::to_bits(state.elapsed), s3::rs::to_bits(1.0f / 60.0f));
    const std::optional<float> written = a.pending_parameter(encode("TrickHeight"));
    CHECK_MSG(written.has_value(), "TU3 HippyJumpAntic must publish TrickHeight");
    CHECK(std::fabs(*written - 2.0f / 60.0f) < 1.0e-6f);
    // Native encoding823C3C78 folds ASCII case, so this spelling aliases
    // the constructor's literal rather than identifying another output.
    CHECK(encode("TrickHeight") == encode("trickheight"));
}
