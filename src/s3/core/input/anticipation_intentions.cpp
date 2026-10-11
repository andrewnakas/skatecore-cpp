// Port of skate-core/src/input/anticipation_intentions.rs.
// Right-stick anticipation publication, Fill825999F0:
// angle/magnitude 82599AD0..9C4C, emissions 8259AD0C..AD40.
#include "s3/core/input/anticipation_intentions.hpp"

#include "s3/core/input/angle.hpp"

namespace s3::input::anticipation_intentions {

using rs::from_bits;

std::vector<RidingIntent> produce(const DerivedControllerInput& controller) {
    const auto& words = controller.words();
    const float x = from_bits(words[9]);
    const float y = from_bits(words[10]);
    if (x == 0.0f && y == 0.0f) {
        return {};
    }
    // Unlike steering, these two records have no actor1908 bit gate.
    const float mag = controller::magnitude(rs::mul_add(x, x, y * y));
    const float ang = angle::left_stick_angle(x, y);
    return {RidingIntent{"AnticMag", mag}, RidingIntent{"AnticAngle", ang}};
}

}  // namespace s3::input::anticipation_intentions
