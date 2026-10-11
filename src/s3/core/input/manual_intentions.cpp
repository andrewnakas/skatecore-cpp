// Port of skate-core/src/input/manual_intentions.rs.
// TU3 Fill825999F0 -> manual producer8259BA28, after Derived input.
#include "s3/core/input/manual_intentions.hpp"

namespace s3::input::manual_intentions {

using rs::from_bits;

std::vector<RidingIntent> produce(const DerivedControllerInput& controller, std::uint32_t actor_flags) {
    std::vector<RidingIntent> intents;
    intents.reserve(2);
    if ((actor_flags & (1u << 9)) != 0) {
        return intents;
    }
    const auto& words = controller.words();
    const float x = from_bits(words[9]);
    const float y = from_bits(words[10]);
    // Fill82599BC8..9C40: x*x + rounded(y*y), then two rsqrt refinements.
    const float length = controller::magnitude(rs::mul_add(x, x, y * y));
    if (y != 0.0f) {
        const float value = y > 0.0f ? length : -length;
        intents.push_back(RidingIntent{"Manual", value});
    }
    // Original constants820997B8/822F9274; strictly greater, independent
    // of the Manual presence gate. A horizontal stick can emit ManualBrake.
    const float threshold = from_bits(0x3f666666u);
    if (length > threshold) {
        const float brake = (length - threshold) * from_bits(0x411ffffeu);
        intents.push_back(RidingIntent{"ManualBrake", y > 0.0f ? brake : -brake});
    }
    return intents;
}

}  // namespace s3::input::manual_intentions
