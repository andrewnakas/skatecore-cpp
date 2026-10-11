// Port of skate-core/src/physics/skateboard_controller.rs.
// Original SkateboardController's lifecycle, TU3 82D74DD8/82D75EA0.
// This is the hand-held/retrieval controller. Normal riding stops it.
#pragma once
#include <cstdint>

#include "s3/core/player/lifecycle.hpp"

namespace s3::physics::skateboard_controller {

using player::lifecycle::SkateboardControllerActions;
using player::lifecycle::SkateboardControllerFields;

struct SkateboardController {
    /// The same mutable fields consumed by PhysicalPlayer state changes.
    //82D74EFC/4F00/4F04, after the retrieval-progress initializer.
    SkateboardControllerFields fields{0, 0, false};

    static constexpr SkateboardController new_() { return SkateboardController{}; }
    static SkateboardController default_() { return new_(); }

    ///82D75EA0. PhysicalPlayer invokes this for requested states outside
    ///500..502. The active-held release remains the real LetGoOfSkateboard
    ///action; do not replace it with a transform reset or an empty callback.
    void stop(SkateboardControllerActions& actions) {
        if (!fields.system_on_452) {
            return;
        }
        const std::uint32_t previous = fields.state_448;
        fields.word_444 = 0;
        if (previous != 0) {
            actions.let_go_of_skateboard();
            fields.state_448 = 0;
        }
        fields.system_on_452 = false;
    }

    ///The player passes this to Skeleton::UpdatePostPhysics. Native868
    ///is the count of seven board parts in contact, not wheel count869.
    bool request_partial_ragdoll(std::uint8_t part_contact_count) const {
        return fields.state_448 == 1 && part_contact_count != 0;
    }
};

}  // namespace s3::physics::skateboard_controller
