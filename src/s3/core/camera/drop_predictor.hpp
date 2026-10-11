// Port of skate-core/src/camera/drop_predictor.rs.
//! Native terrain drop prediction82DF84F8/89B0/8BC0/8740.
//! Retains one update of query history: a new batch is submitted only after the
//! previous batch has been consumed. Host execution may be synchronous.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/positioner.hpp"

namespace s3::camera {

struct DropSettings {
    /// camera_droppredictor/default fields bound8252EF84..8252EFA4.
    float minimum_test_distance = 0.0f;
    float total_test_time = 0.0f;
    float maximum_test_distance = 0.0f;
    float maximum_drop_distance = 0.0f;
};
S3_DERIVE_EQ(DropSettings, minimum_test_distance, total_test_time, maximum_test_distance, maximum_drop_distance)

struct DropCollisionProvider {
    virtual ~DropCollisionProvider() = default;
    virtual rs::Result<std::array<FatLineResult, 10>, std::string> query(std::array<FatLine, 10> lines,
                                                                         std::uint32_t context) = 0;
};

namespace drop_predictor {
/// private in Rust.
struct Probe {
    F4 start{};
    F4 end{};
    F4 normal{};
    float depth = 0.0f;
    bool valid = false;
    bool hit = false;
};
S3_DERIVE_EQ(Probe, start, end, normal, depth, valid, hit)
}  // namespace drop_predictor

struct DropPredictor {
    F4 horizontal_velocity{};
    F4 steepest_normal{};
    bool has_normal = false;
    float elevation = 0.0f;
    float distance_offset = 0.0f;
    float target_elevation = 0.0f;
    // private in Rust:
    float spacing = 0.0f;
    std::size_t selected = 0;
    std::array<drop_predictor::Probe, 5> probes{};
    std::optional<std::pair<std::array<FatLine, 10>, std::array<FatLineResult, 10>>> pending;

    static DropPredictor new_() { return DropPredictor{}; }

    /// Reset82DF8440 preserves the five probe records and cancels their pending
    /// query. The following disabled/reset update clears probe valid/depth.
    void reset();

    /// Complete82E00B60 velocity selection, consuming the previous rig velocity.
    static F4 prediction_velocity(bool reset, const F4x4& transform, F4 rig_velocity, DropSettings settings);

    rs::Result<rs::Unit, std::string> update(F4 position, F4 velocity, bool enabled, bool offboard,
                                             std::uint32_t context, DropSettings settings,
                                             DropCollisionProvider& query);

    /// Native getter82DF8980.
    float drop_depth() const;

private:
    std::array<FatLine, 10> lines(F4 position, float depth) const;
    void consume(const std::array<FatLine, 10>& lines, const std::array<FatLineResult, 10>& results,
                 float maximum_depth);
    void update_elevation(bool enabled, bool offboard);
};
S3_DERIVE_EQ(DropPredictor, horizontal_velocity, steepest_normal, has_normal, elevation, distance_offset,
             target_elevation, spacing, selected, probes, pending)

}  // namespace s3::camera
