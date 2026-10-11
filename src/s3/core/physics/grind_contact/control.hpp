// Port of skate-core/src/physics/grind_contact/control.rs.
// TU3 one-truck control82D89F58 and orientation82D40290.
#pragma once
#include "s3/core/physics/grind_contact.hpp"

namespace s3::physics::grind_contact::control {

struct Control {
    float yaw = 0.0f;
    float pitch = 0.0f;

    ///Animation2796/2800/2804/2808; candidate412 bit29 selects the front end.
    void update(std::uint32_t kind, V board_forward, V normal, bool front, bool switched, bool hanging_back,
                float translation, float nudge, float up_down, float grab_min_height);
};

V rotate(V axis, V value, float angle);

///82D40290: yaw around support, orthogonalize, then pitch around deck right.
std::array<V, 4> truck_frame(std::array<V, 4> board, V normal, const Control& control, bool switched);

///82D42528/82D42AF8: rotate support by20/42 degrees about the directed rail,
///then align board-right with the rail. Side is determined by contact offset.
std::array<V, 4> tip_frame(std::array<V, 4> board, V direction, V normal, V point, bool backslash);

}  // namespace s3::physics::grind_contact::control
