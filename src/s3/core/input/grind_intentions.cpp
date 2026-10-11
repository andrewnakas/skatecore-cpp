// Port of skate-core/src/input/grind_intentions.rs.
// Original S3 Fill825999F0; emissions8259ACF0..8259AD94.
#include "s3/core/input/grind_intentions.hpp"

namespace s3::input::grind_intentions {

using rs::from_bits;

std::vector<RidingIntent> produce(const DerivedControllerInput& controller) {
    const auto& words = controller.words();
    const float left_x = from_bits(words[7]);
    const float right_x = from_bits(words[9]);
    const float right_y = from_bits(words[10]);
    // S3 adds both X axes. S2 Fill825FB4C8 uses right X alone: do not copy it.
    const float sum = left_x + right_x;
    const float lower = -1.0f - sum >= 0.0f ? -1.0f : sum;
    const float translation = 1.0f - lower >= 0.0f ? lower : 1.0f;
    const RidingIntent all[4] = {
        {"GrindBalanceX", -left_x},
        {"PhysGrindTranslation", translation},
        {"PhysGrindStabilityNudge", left_x},
        {"PhysGrindUpDown", right_y},
    };
    std::vector<RidingIntent> out;
    for (const RidingIntent& i : all) {
        if (i.value != 0.0f) out.push_back(i);
    }
    return out;
}

}  // namespace s3::input::grind_intentions
