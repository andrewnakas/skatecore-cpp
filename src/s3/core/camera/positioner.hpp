// Port of skate-core/src/camera/positioner.rs.
//! TU3 normal-camera positioner 82E01948 and its complete collision/geometry
//! callees. The caller supplies recovered state/configuration and the native
//! six-fat-line backend; this module does not choose camera settings.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/tracker.hpp"

namespace s3::camera {

struct FatLine {
    F4 start{};
    F4 end{};
    float radius = 0.0f;
};
S3_DERIVE_EQ(FatLine, start, end, radius)

/// Native 48-byte result: vectors0/16, fraction32, hit byte36, surface word40.
struct FatLineResult {
    F4 position{};
    F4 normal{};
    float fraction = 0.0f;
    std::uint8_t hit = 0;
    std::uint32_t surface = 0;
};
S3_DERIVE_EQ(FatLineResult, position, normal, fraction, hit, surface)

/// Camera server slots+8/+12, implemented natively by82DF31C0/82DF3290.
/// Results preserve segment order; native submission always contains six lines.
struct PositionerCollisionProvider {
    virtual ~PositionerCollisionProvider() = default;
    virtual void submit(std::array<FatLine, 6> lines, std::uint32_t context) = 0;
    virtual std::array<FatLineResult, 6> results() = 0;
};

struct PositionerConfig {
    F4 anchor{};
    F4 offset{};
    float reference_heading = 0.0f;
    float reference_elevation = 0.0f;
    float heading = 0.0f;
    float elevation = 0.0f;
    float distance = 0.0f;
    float minimum_distance = 0.0f;
    std::uint8_t collision_enabled = 0;
    std::uint8_t bypass_distance_tracker = 0;
    std::uint8_t clamp_height = 0;
    float floor_height = 0.0f;
    float speed_clamp = 0.0f;
    float acceleration_clamp = 0.0f;
    float smoothing = 0.0f;
    std::array<F4, 3> query_starts{};
};
S3_DERIVE_EQ(PositionerConfig, anchor, offset, reference_heading, reference_elevation, heading, elevation, distance,
             minimum_distance, collision_enabled, bypass_distance_tracker, clamp_height, floor_height, speed_clamp,
             acceleration_clamp, smoothing, query_starts)

struct Positioner {
    ScalarTracker distance_tracker;
    ScalarTrackerParameters tracker_parameters;
    F4 offset{};
    F4 previous_valid_position{};
    F4 reference_position{};
    float collision_clear_time = 0.0f;
    PositionerConfig config;
    F4 position{};
    F4 velocity{};
    float distance = 0.0f;
    float heading = 0.0f;
    float elevation = 0.0f;
    float radius = 0.0f;
    float available_distance = 0.0f;
    std::uint8_t flags = 0;

    /// Complete82E01948. Negative/zero dt follows native tracker/velocity gates.
    std::uint32_t update(float dt, std::uint32_t context, PositionerConfig config,
                         PositionerCollisionProvider& collision);
    /// Complete82E01BA0. Query status 3 preserves previous-valid state; the
    /// current complete82E01EA8 body produces only statuses0,1,2.
    std::uint32_t resolve(std::uint32_t context, PositionerCollisionProvider& collision);
    /// Complete82E01D30. Submit happens even when collision_enabled is zero.
    void submit_collision(std::uint32_t context, PositionerCollisionProvider& collision);
    /// Complete82E01EA8, preserving first-three/last-three result roles and order.
    std::uint32_t resolve_collision(std::uint32_t context, PositionerCollisionProvider& collision);

private:
    F4 position_at(float elevation, float heading, float distance) const;
};
S3_DERIVE_EQ(Positioner, distance_tracker, tracker_parameters, offset, previous_valid_position, reference_position,
             collision_clear_time, config, position, velocity, distance, heading, elevation, radius,
             available_distance, flags)

/// Complete82E020F0. Invalid hit positions or a zero hit byte preserve the end.
F4 project_hit(F4 end, F4 first_query_start, FatLineResult result);

/// Complete82E02190, including native four-lane basis construction and ordering.
F4 position_from_angles(F4 anchor, F4 offset, float elevation, float heading, float distance,
                        std::uint8_t clamp_height, float minimum_height);

}  // namespace s3::camera
