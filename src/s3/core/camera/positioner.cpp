// Port of skate-core/src/camera/positioner.rs.
#include "s3/core/camera/positioner.hpp"

#include "s3/core/camera/vector_tracker.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::camera {

using vector_tracker::length;
using vector_tracker::refined_reciprocal;

std::uint32_t Positioner::update(float dt, std::uint32_t context, PositionerConfig config_,
                                 PositionerCollisionProvider& collision) {
    this->config = config_;
    this->offset = config_.offset;
    if (this->collision_clear_time < 5.0f) {
        const float scale = this->collision_clear_time * rs::from_bits(0x3e4ccccd);
        this->offset = rs::map(this->offset, [&](float v) { return v * scale; });
    }
    if (config_.bypass_distance_tracker != 0) {
        this->distance_tracker.target = config_.distance;
        this->distance_tracker.position = config_.distance;
    } else {
        const float acceleration =
            this->distance_tracker.position < rs::from_bits(0x3f866666) ? 100.0f : config_.acceleration_clamp;
        this->tracker_parameters.acceleration_clamp_min = acceleration;
        this->tracker_parameters.acceleration_clamp_max = acceleration;
        this->tracker_parameters.speed_clamp = config_.speed_clamp;
        this->tracker_parameters.smoothing_min = config_.smoothing;
        this->tracker_parameters.smoothing_max = config_.smoothing;
        this->distance_tracker.update(dt, config_.distance, this->tracker_parameters);
    }
    this->radius = rs::from_bits(0x3e051eb8);
    const F4 previous = this->position;
    const std::uint32_t result = this->resolve(context, collision);
    if (dt > 0.0f) {
        const float inverse = refined_reciprocal(dt);
        this->velocity = rs::from_fn<4>([&](std::size_t i) { return inverse * (this->position[i] - previous[i]); });
    }
    if ((this->flags & 0xc0) != 0) {
        this->collision_clear_time = 0.0f;
    } else {
        this->collision_clear_time += dt;
    }
    return result;
}

std::uint32_t Positioner::resolve(std::uint32_t context, PositionerCollisionProvider& collision) {
    this->available_distance = this->config.distance;
    this->flags = std::uint8_t((this->flags & 0xc1) | 0x0c);
    this->heading = this->config.heading;
    this->elevation = this->config.elevation;
    this->distance = this->distance_tracker.position;
    this->position = this->position_at(this->elevation, this->heading, this->distance);
    const std::uint32_t result = this->resolve_collision(context, collision);
    if (result == 1 || result == 2) {
        const float d =
            length(rs::from_fn<4>([&](std::size_t i) { return this->position[i] - this->config.anchor[i]; }));
        this->distance_tracker.target = d;
        this->distance_tracker.position = d;
        this->distance_tracker.velocity = 0.0f;
        this->distance_tracker.acceleration = 0.0f;
        this->distance = d;
    }
    if (result == 3) {
        this->position = this->previous_valid_position;
        this->flags &= std::uint8_t(~8u);
    } else {
        this->previous_valid_position = this->position;
        this->flags = std::uint8_t((this->flags & ~0x20u) | (result == 2 ? 0x20u : 0u));
    }
    this->flags = std::uint8_t((this->flags & 0x3f) | (result != 0 ? 0x80u : 0u));
    return result;
}

void Positioner::submit_collision(std::uint32_t context, PositionerCollisionProvider& collision) {
    this->reference_position =
        this->position_at(this->config.reference_elevation, this->config.reference_heading, this->config.distance);
    const std::array<FatLine, 6> lines = rs::from_fn<6>([&](std::size_t i) {
        return FatLine{
            this->config.query_starts[i % 3],
            i < 3 ? this->position : this->reference_position,
            this->radius,
        };
    });
    collision.submit(lines, context);
}

std::uint32_t Positioner::resolve_collision(std::uint32_t context, PositionerCollisionProvider& collision) {
    this->submit_collision(context, collision);
    const std::array<FatLineResult, 6> results = collision.results();
    if (this->config.collision_enabled != 0) {
        this->flags |= 2;
        this->available_distance = 0.0f;
        for (std::size_t k = 3; k < 6; ++k) {
            const FatLineResult& result = results[k];
            if (result.hit != 0) {
                const F4 point = project_hit(this->reference_position, this->config.query_starts[0], result);
                const float d =
                    length(rs::from_fn<4>([&](std::size_t i) { return point[i] - this->config.anchor[i]; }));
                if (!(this->available_distance - d >= 0.0f)) {
                    this->available_distance = d;
                }
            } else {
                this->available_distance = this->config.distance;
                this->flags &= std::uint8_t(~2u);
            }
        }
        if (results[0].hit != 0 && results[1].hit != 0 && results[2].hit != 0) {
            this->position = project_hit(this->position, this->config.query_starts[0], results[0]);
            const float d = length(
                rs::from_fn<4>([&](std::size_t i) { return this->position[i] - this->config.query_starts[0][i]; }));
            return d < this->radius + this->config.minimum_distance ? 2 : 1;
        }
    }
    return 0;
}

F4 Positioner::position_at(float elevation_, float heading_, float distance_) const {
    return position_from_angles(this->config.anchor, this->offset, elevation_, heading_, distance_,
                                this->config.clamp_height, this->radius + this->config.floor_height);
}

F4 project_hit(F4 end, F4 first_query_start, FatLineResult result) {
    if (!rs::is_nan(result.position[0]) && !rs::is_nan(result.position[1]) && !rs::is_nan(result.position[2]) &&
        result.hit != 0) {
        const float lower = -result.fraction >= 0.0f ? 0.0f : result.fraction;
        const float fraction = 1.0f - lower >= 0.0f ? lower : 1.0f;
        return rs::from_fn<4>(
            [&](std::size_t i) { return rs::mul_add(end[i] - first_query_start[i], fraction, first_query_start[i]); });
    }
    return end;
}

F4 position_from_angles(F4 anchor, F4 offset, float elevation, float heading, float distance,
                        std::uint8_t clamp_height, float minimum_height) {
    const auto se_ce = trigonometry::sin_cos(-elevation);
    const float s_e = se_ce.first;
    const float c_e = se_ce.second;
    const auto sh_ch = trigonometry::sin_cos(-heading);
    const float s_h = sh_ch.first;
    const float c_h = sh_ch.second;
    // 822FB890 permutation and vrlimi lane insertion, in guest memory order.
    const std::array<F4, 3> first = {{
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, c_e, s_e, 0.0f},
        {0.0f, -s_e, c_e, 0.0f},
    }};
    const std::array<F4, 3> second = {{
        {c_h, 0.0f, -s_h, c_h},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {s_h, 0.0f, c_h, s_h},
    }};
    const std::array<F4, 3> product = rs::from_fn<3>([&](std::size_t column) {
        return rs::from_fn<4>([&](std::size_t i) {
            float value = second[0][i] * first[column][0];
            value = rs::mul_add(second[1][i], first[column][1], value);
            return rs::mul_add(second[2][i], first[column][2], value);
        });
    });
    F4 point = rs::from_fn<4>([&](std::size_t i) {
        float value = product[0][i] * 0.0f;
        value = rs::mul_add(product[1][i], 0.0f, value);
        value = rs::mul_add(product[2][i], distance, value);
        return (value + offset[i]) + anchor[i];
    });
    if (clamp_height != 0 && minimum_height > point[1]) {
        point[1] = minimum_height;
    }
    return point;
}

}  // namespace s3::camera
