// Port of skate-game/src/physics/biped_ground/grab_runtime/selection.rs
//! Ground mode0 consumer of the original core82D4D150 implementation.
#include "s3/game/physics/biped_ground/grab_runtime/selection.hpp"

namespace s3::game::physics::biped_ground::grab_runtime::selection {

std::optional<Record> best(const std::vector<Record>& records, std::array<float, 4> position) {
    return ::s3::player::offboard::grab_scene::best_spline(records, position);
}

}  // namespace s3::game::physics::biped_ground::grab_runtime::selection
