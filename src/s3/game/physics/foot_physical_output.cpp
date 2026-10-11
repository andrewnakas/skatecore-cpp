// Port of skate-game/src/physics/foot_physical_output.rs.
#include "s3/game/physics/foot_physical_output.hpp"

namespace s3::game::physics::foot_physical_output {

rs::Result<FootPhysicalOutputs, std::string> FootPhysicalOutputs::load(const sd::Collections& data) {
    FootPhysicalOutputs out;
    out.state = sc::physics::foot_physical_output::FootPhysicalState{};
    out.output = sc::physics::foot_physical_output::FootPhysicalOutput{};
    S3G_TRY(const float width, data.float_("physicsdeck", "default", "DeckWidth"));
    S3G_TRY(const float front, data.float_("physicsdeck", "default", "DeckFrontEndSize"));
    S3G_TRY(const float mid, data.float_("physicsdeck", "default", "DeckMidLength"));
    S3G_TRY(const auto padding, data.words<4>("physics_skeletonik", "default", "FootOnDeckPadding"));
    out.settings.deck_half_width = width * 0.5f;
    out.settings.deck_total_half_length = front + mid * 0.5f;
    out.settings.padding = rs::from_bits(padding);
    return out;
}

sc::physics::foot_physical_output::FootPhysicalOutput FootPhysicalOutputs::publish(
    const sc::physics::skeleton_body::SkeletonPhysicalRecord& record, float dt) {
    output = state.update(record, dt, settings);
    return output;
}

}  // namespace s3::game::physics::foot_physical_output
