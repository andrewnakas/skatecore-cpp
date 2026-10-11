// Port of skate-core/src/camera/anchors.rs.
//! Seven normal-camera anchors created by TU382DF2DB8 and updated82DF69C0.
//! Names come from that constructor; notably HipsAnchor reads reckoned COM.
#pragma once
#include <array>

#include "s3/core/camera/_types.hpp"

namespace s3::camera {

struct AnchorInputs {
    /// Subject388: physical output entry0+144.
    F4 board_position{};
    /// Subject392/396: board output entry1+80/+64 respectively.
    F4 board_velocity{};
    F4 board_acceleration{};
    /// Subject456/460/464: reckoning direction, COM and filtered COM.
    F4 board_offset_direction{};
    F4 center_of_mass{};
    F4 damped_center_of_mass{};
    /// Subject384 matrix+16 and436.
    F4 skeleton_root_up{};
    F4 grind_point{};
    /// Subject596/544; reset is state output byte62.
    bool grinding = false;
    bool reset = false;
};
S3_DERIVE_EQ(AnchorInputs, board_position, board_velocity, board_acceleration, board_offset_direction,
             center_of_mass, damped_center_of_mass, skeleton_root_up, grind_point, grinding, reset)

struct AnchorState {
    F4 position{};
    F4 velocity{};
    F4 acceleration{};
};
S3_DERIVE_EQ(AnchorState, position, velocity, acceleration)

struct Anchors {
    /// Skateboard, Hips, Air, Grind, World, DampedCOM, POI in native order.
    std::array<AnchorState, 7> entries{};
    F4 grind_position{};  // private in Rust

    static Anchors new_() { return Anchors{}; }
    void update(const AnchorInputs& input);
};
S3_DERIVE_EQ(Anchors, entries, grind_position)

}  // namespace s3::camera
