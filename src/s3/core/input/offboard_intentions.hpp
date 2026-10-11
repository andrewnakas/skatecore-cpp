// Port of skate-core/src/input/offboard_intentions.rs.
// Off-board inputs from original TU3 Fill825999F0 and helper8259C260.
// The stock ActionGraph owns activation; these inputs do not change state.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string_view>
#include <vector>

#include "s3/core/input/controller.hpp"
#include "s3/core/rs.hpp"

namespace s3::input::offboard_intentions {

using controller::DerivedControllerInput;

struct OffboardIntent {
    std::string_view name;
    float value = 0.0f;
};
S3_DERIVE_EQ(OffboardIntent, name, value)
inline std::ostream& operator<<(std::ostream& os, const OffboardIntent& i) {
    return os << "(\"" << i.name << "\", " << i.value << ")";
}

/// Read the already-updated controller prefix exactly once per input tick.
/// `air_reckoning_active` is actual PhysOut bundle+8 byte452, not a state test.
/// Analog movement has a separate physical-output-dependent producer.
std::vector<OffboardIntent> produce_discrete(const DerivedControllerInput& controller, std::uint32_t actor_flags,
                                             bool air_reckoning_active);

/// Completed physical publication read by Fill825999F0.
struct AnalogObservation {
    /// PhysOutSkeleton+0: effective animation root Z, including processed2476bit2.
    std::array<float, 4> effective_skeleton_z{};
    /// Some(OffBoard288) iff actual OffBoard333 is nonzero. This vector is not
    /// normalized here. A missing Biped producer must not be converted to None.
    std::optional<std::array<float, 4>> biped_correction;
};
S3_DERIVE_EQ(AnalogObservation, effective_skeleton_z, biped_correction)

///8259A7A8..A9BC calculation and8259AFFC..B088 ordered movement publications.
std::array<OffboardIntent, 4> produce_analog(const DerivedControllerInput& controller,
                                             AnalogObservation observation);

}  // namespace s3::input::offboard_intentions
