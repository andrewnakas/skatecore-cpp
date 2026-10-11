// Port of skate-game/src/graph_host/motion_air_execute.rs (module `motion::air_execution`).
#include "s3/game/graph_host/motion/air_execution.hpp"

#include <algorithm>

#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion/execution.hpp"
#include "s3/game/graph_host/motion_air_leg.hpp"

namespace s3::game::graph_host::motion {

rs::Result<rs::Unit, std::string> MotionHost::execute_air(BehaviorId behavior, MotionOperation operation,
                                                          const Frame& frame, std::uint8_t phase) {
    using s3::animation::playback_parameters::SettableAttribute;
    using s3::animation::skeleton_input::name::encode;
    if (behavior >= instances.size()) {
        return rs::make_err(std::string("Unallocated airborne MotionGraph behavior"));
    }
    Instance& instance = instances[behavior];
    if (operation.is<MotionOperation::ClearTrickAttr>()) {
        //End82BB5DB0 -> IAnimatable112 (8231E050+112) ->82531278:
        //erase the construction-map key. Begin/Update are82B61BB8.
        if (phase == 2) {
            const auto trick = encode("Trick");
            auto& values = animation.construction_values;
            values.erase(std::remove_if(values.begin(), values.end(),
                                        [&](const auto& entry) { return entry.first == trick; }),
                         values.end());
        }
        return rs::Unit{};
    }
    const auto* air_leg_op = std::get_if<MotionOperation::AirLeg>(&operation.value);
    auto* air_leg_state = instance.get<Instance::AirLeg>();
    if (air_leg_op != nullptr && air_leg_state != nullptr) {
        auto& state = air_leg_state->_0;
        const auto& op = air_leg_op->_0;
        if (phase == 1) {
            S3_MHOST_SOME(const auto physical_, air_leg_physical,
                           "Air leg extension requires completed physical output");
            std::optional<s3::animation::output::attributes::AnimationAttribute> initial;
            if (state.needs_initial_attribute()) {
                S3_MHOST_TRY(initial, animation.last_attribute(op.height));
            }
            bool prepare = false;
            if (state.needs_prelanding_query(physical_)) {
                S3_MHOST_SOME(const auto prelanding, prelanding_physical,
                               "Air leg extension requires actual prelanding output");
                const bool query = prelanding.near_landing(spin);
                std::optional<s3::animation::output::attributes::AnimationAttribute> full;
                if (query) {
                    S3_MHOST_TRY(full, animation.last_attribute(encode("FullExtension")));
                }
                prepare = s3::animation::air_leg_extension::prepare_to_land(query, full, physical_, air_leg);
            }
            const std::array<float, 2> values =
                state.update(op.bone, physical_, air_leg, frame.dt, initial, prepare);
            animation.set_attribute(SettableAttribute{op.height, values[0], false, -1});
            animation.set_attribute(SettableAttribute{encode("extend"), values[1], false, -1});
        }
        return rs::Unit{};
    }
    auto* spin_state = instance.get<Instance::BodySpin>();
    if (operation.is<MotionOperation::BodySpin>() && spin_state != nullptr) {
        if (phase == 1) {
            if (!condition_inputs.physical_state) {
                return rs::make_err(std::string("BodySpin requires physicalcategory"));
            }
            const std::uint32_t category = condition_inputs.physical_state->category;
            S3_MHOST_SOME(const auto gameplay, gameplay_conditions, "BodySpin requires physicalstate");
            S3_MHOST_SOME(const bool mirrored, playback_context.is_mirrored, "BodySpin requires animstance");
            S3_MHOST_CHECK(spin_state->_0.update(animation, category, gameplay.state, flags.doing_trick,
                                                  hand_services.busy_hands, mirrored, prelanding_physical, spin));
        }
        return rs::Unit{};
    }
    return rs::make_err(std::string("Airborne MotionGraph operation/instance mismatch"));
}

}  // namespace s3::game::graph_host::motion
