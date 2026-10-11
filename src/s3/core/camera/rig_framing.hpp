// Port of skate-core/src/camera/rig_framing.rs.
//! Complete TU3 framing82E03560 and subject-driven mode selection82E053E0.
#pragma once
#include <cstdint>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/positioner.hpp"
#include "s3/core/camera/rig_tracking.hpp"

namespace s3::camera {

struct RigFraming {
    F4 anchor{};
    F4 offset{};
    float elevation_offset = 0.0f;
    float additional_elevation = 0.0f;
    float reference_height = 0.0f;
    float distance = 0.0f;

    /// The threshold is the native global vector830BD350. Its runtime producer
    /// must be bound by the caller; mapped-image zeros are not a tuning default.
    void update(float dt, AngularRigTracking& angular, F4 tracked_anchor, const Positioner& positioner,
                F4 normalization_threshold);
};
S3_DERIVE_EQ(RigFraming, anchor, offset, elevation_offset, additional_elevation, reference_height, distance)

struct RigModeSubject {
    virtual ~RigModeSubject() = default;
    virtual std::uint8_t flag_568() = 0;
    virtual std::uint8_t flag_596() = 0;
    virtual std::uint8_t flag_680() = 0;
    virtual std::uint8_t flag_540() = 0;
};

struct RigMode {
    std::uint32_t current = 0;
    std::uint32_t previous = 0;
    float elapsed = 0.0f;

    /// Complete82E053E0. No graph topology or skater-state mapping is inferred
    /// from the native getter slots. Preserve their conditional call order.
    void update(RigModeSubject& subject);
};
S3_DERIVE_EQ(RigMode, current, previous, elapsed)

}  // namespace s3::camera
