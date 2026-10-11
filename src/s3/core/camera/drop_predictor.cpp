// Port of skate-core/src/camera/drop_predictor.rs.
#include "s3/core/camera/drop_predictor.hpp"

#include "s3/core/camera/orientation_math.hpp"
#include "s3/core/camera/vector_tracker.hpp"
#include "s3/core/input/angle.hpp"

namespace s3::camera {

using vector_tracker::length;

void DropPredictor::reset() {
    this->horizontal_velocity = F4{};
    this->steepest_normal = F4{};
    this->has_normal = false;
    this->elevation = 0.0f;
    this->distance_offset = 0.0f;
    this->target_elevation = 0.0f;
    this->spacing = 0.0f;
    this->selected = 0;
    this->pending = std::nullopt;
}

F4 DropPredictor::prediction_velocity(bool reset, const F4x4& transform, F4 rig_velocity, DropSettings settings) {
    if (reset || settings.minimum_test_distance > length(rig_velocity)) {
        return rs::map(transform[2], [&](float v) { return v * settings.minimum_test_distance; });
    }
    return rig_velocity;
}

rs::Result<rs::Unit, std::string> DropPredictor::update(F4 position, F4 velocity, bool enabled, bool offboard,
                                                        std::uint32_t context, DropSettings settings,
                                                        DropCollisionProvider& query) {
    this->horizontal_velocity = {velocity[0], 0.0f, velocity[2], 0.0f};
    if (enabled) {
        float span = length(this->horizontal_velocity) * settings.total_test_time;
        span = -span >= 0.0f ? 0.0f : span;
        span = settings.maximum_test_distance - span >= 0.0f ? span : settings.maximum_test_distance;
        this->spacing = span / 5.0f;
        if (this->pending.has_value()) {
            auto taken = std::move(*this->pending);
            this->pending = std::nullopt;
            this->consume(taken.first, taken.second, settings.maximum_drop_distance);
        }
        const std::array<FatLine, 10> lines_ = this->lines(position, settings.maximum_drop_distance);
        S3_CAM_TRY(auto results, query.query(lines_, context));
        this->pending = std::make_pair(lines_, results);
        float greatest_depth = 0.0f;
        this->selected = 0;
        for (std::size_t index = 0; index < this->probes.size(); ++index) {
            const drop_predictor::Probe& probe = this->probes[index];
            if (!probe.valid) {
                break;
            }
            if (probe.depth > greatest_depth) {
                this->selected = index;
                greatest_depth = probe.depth;
            }
        }
    } else {
        this->selected = 0;
        for (drop_predictor::Probe& probe : this->probes) {
            probe.valid = false;
            probe.depth = 0.0f;
        }
    }
    this->update_elevation(enabled, offboard);
    return rs::Unit{};
}

float DropPredictor::drop_depth() const {
    const drop_predictor::Probe& probe = this->probes[this->selected];
    return probe.valid ? probe.depth : 0.0f;
}

std::array<FatLine, 10> DropPredictor::lines(F4 position, float depth) const {
    const F4 direction = orientation_math::normalize(this->horizontal_velocity);
    return rs::from_fn<10>([&](std::size_t index) {
        const F4 point = rs::from_fn<4>([&](std::size_t i) {
            return rs::mul_add(direction[i] * this->spacing, float(index / 2 + 1), position[i]);
        });
        if (index % 2 == 0) {
            return FatLine{position, point, rs::from_bits(0x3c23d70a)};
        }
        const F4 down = {0.0f, 1.0f, 0.0f, 0.0f};
        return FatLine{point, rs::from_fn<4>([&](std::size_t i) { return point[i] - down[i] * depth; }),
                       rs::from_bits(0x3dcccccd)};
    });
}

void DropPredictor::consume(const std::array<FatLine, 10>& lines_, const std::array<FatLineResult, 10>& results,
                            float maximum_depth) {
    this->has_normal = false;
    for (std::size_t index = 0; index < 5; ++index) {
        if (results[2 * index].hit != 0) {
            for (std::size_t k = index; k < 5; ++k) {
                this->probes[k].valid = false;
                this->probes[k].depth = 0.0f;
            }
            break;
        }
        const FatLine& line = lines_[2 * index + 1];
        const FatLineResult& result = results[2 * index + 1];
        drop_predictor::Probe& probe = this->probes[index];
        probe = drop_predictor::Probe{line.start, line.end, F4{}, maximum_depth, true, result.hit != 0};
        if (probe.hit) {
            probe.depth = length(rs::from_fn<4>([&](std::size_t i) { return result.position[i] - line.start[i]; }));
            probe.end = result.position;
            probe.normal = result.normal;
            if (!this->has_normal || rs::abs(this->steepest_normal[1]) > rs::abs(result.normal[1])) {
                this->steepest_normal = result.normal;
                this->has_normal = true;
            }
        }
    }
}

void DropPredictor::update_elevation(bool enabled, bool offboard) {
    const drop_predictor::Probe probe = this->probes[this->selected];
    float angle = 0.0f;
    if (probe.valid) {
        const float distance = float(this->selected + 1) * this->spacing;
        // Native angle wrapper applies one reciprocal refinement here; the
        // called82473B98 polynomial has its own independent reductions.
        const float initial = physics::native_arithmetic::reciprocal_estimate(distance);
        const float inverse = rs::mul_add(initial, rs::mul_add(-initial, distance, 1.0f), initial);
        angle = input::angle::atan(rs::mul_add(probe.depth, inverse, 0.0f));
        const std::uint32_t sign = rs::to_bits(probe.depth) & 0x80000000u;
        if (0.0f > distance) {
            angle = rs::from_bits(0x40490fdbu | sign) + angle;
        }
        if (distance == 0.0f) {
            angle = rs::from_bits(0x3fc90fdbu | sign);
        }
    }
    const float half_pi = rs::from_bits(0x3fc90fdb);
    if (angle - half_pi >= 0.0f) {
        angle = half_pi;
    }
    const float limit = (offboard ? 40.0f : 70.0f) * rs::from_bits(0x3c8efa35);
    if (angle - limit >= 0.0f) {
        angle = limit;
    }
    this->target_elevation = (enabled && angle >= rs::from_bits(0x3e860a92)) ? angle : 0.0f;
    const float delta = rs::abs(this->target_elevation - this->elevation);
    const float degrees = delta * rs::from_bits(0x42652ee1);
    float step = rs::mul_add(degrees - 7.0f, rs::from_bits(0x3d3a2e8c), rs::from_bits(0x3dcccccd));
    step = rs::from_bits(0x3dcccccd) - step >= 0.0f ? rs::from_bits(0x3dcccccd) : step;
    step = rs::from_bits(0x3fcccccd) - step >= 0.0f ? step : rs::from_bits(0x3fcccccd);
    if (degrees > step) {
        this->elevation = this->elevation >= this->target_elevation
                              ? -rs::mul_add(step, rs::from_bits(0x3c8efa35), -this->elevation)
                              : rs::mul_add(step, rs::from_bits(0x3c8efa35), this->elevation);
    } else {
        this->elevation = this->target_elevation;
    }
}

}  // namespace s3::camera
