// Port of skate-core/src/air/trajectory/grind_surface/landing.rs.
// Original S3 TU3 82C21930 after six-query completion, not the S2 type3 gate.
// Query ownership stays with the caller; None means actual failed completion.
#pragma once
#include "s3/core/air/trajectory/grind_surface.hpp"

namespace s3::air::trajectory::grind_surface::landing {

/// Native PossibleGrindOrientations output offsets. The returned up-vector is
/// separate: 82C21930 can leave it and these vectors untouched on failure.
struct LandingOrientation {
    GeometryType kind = GeometryType::ThinRail;  // +0
    bool garbage = false;                        // +4
    V boardslide_dir{};                          // +16
    V tipslide_dir{};                            // +32
    V backslash_dir{};                           // +48
    V high_side{};                               // +64
};

/// 82C21930: primitive endpoints and grind_position form a six-probe geometry
/// investigation (optional_probe=None). Pass its completed surface as Some;
/// when prepare returns None, pass None and retain the caller's cached vectors.
/// takeoff_position is original r5, grind_position r6; neither is a velocity.
/// Completed kind3 is deliberately NOT rejected here; the analyzer owns its
/// later type3 rejection. Do not replace it with S2 82CFCA88's early type gate.
/// (`surface` is Option<&GrindSurface>: nullptr == None.)
void update_landing_orientation(const GrindSurface* surface, V takeoff_position, V grind_position, V& up,
                                LandingOrientation& out);

}  // namespace s3::air::trajectory::grind_surface::landing

namespace s3::air::trajectory::grind_surface {
// pub use landing::{LandingOrientation, update_landing_orientation};
using landing::LandingOrientation;
using landing::update_landing_orientation;
}  // namespace s3::air::trajectory::grind_surface
