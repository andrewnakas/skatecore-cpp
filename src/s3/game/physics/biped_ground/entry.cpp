// Port of skate-game/src/physics/biped_ground/entry.rs
//! Effective animation root82BE3650, called by Ground Enter at82D30848.
#include "s3/game/physics/biped_ground/entry.hpp"

namespace s3::game::physics::biped_ground::entry {

///The source multiplies every lane of X/Z by -1, not the position or up.
Frame effective_root(Frame frame, std::uint32_t flags_2476) {
    if ((flags_2476 & 4u) != 0) {
        for (std::size_t axis : {std::size_t(0), std::size_t(2)}) {
            for (float& v : frame[axis]) v = v * -1.0f;
        }
    }
    return frame;
}

}  // namespace s3::game::physics::biped_ground::entry
