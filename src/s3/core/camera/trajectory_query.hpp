// Port of skate-core/src/camera/trajectory_query.rs.
//! Camera-observable part of the TU3 trajectory batch. The variable-step walk
//! is 82770910, step sizing 8276C940 and collision time 8276C558. World storage
//! and scheduling are host-owned; the native segment sequence is retained.
#pragma once
#include <optional>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera {

struct TrajectoryQuery {
    F4 position{};
    F4 velocity{};
    F4 gravity{};
    float duration = 0.0f;
    float radius = 0.0f;
    float start_error = 0.0f;
    float end_error = 0.0f;

    /// Return the first trajectory-segment collision's reconstructed native
    /// time, or -1. The callback returns the closest swept-line contact point.
    /// The extra normal/mesh/frame fields of the native batch are not consumed
    /// by the gameplay camera request's GetCollisionTime accessor82DF9340.
    /// `line(start, end, radius) -> rs::Result<std::optional<F4>, E>`.
    template <class E, class Line>
    rs::Result<float, E> collision_time(Line&& line) const {
        float start_step = this->duration;
        float end_step = this->duration;
        if (this->gravity[1] < 0.0f) {
            start_step = this->step_size(this->start_error * this->radius);
            end_step = this->step_size(this->end_error * this->radius);
        }
        const float minimum_square = ((this->start_error * this->radius) * this->start_error) * this->radius;
        float time = 0.0f;
        F4 start = this->evaluate(time);
        F4 end = this->evaluate(time + start_step);
        while (this->duration > time) {
            const F4 delta = rs::from_fn<4>([&](std::size_t i) { return end[i] - start[i]; });
            if (vector_tracker::dot(delta, delta) > minimum_square) {
                // 82770B40 rejects only segments with every XYZ component at
                // or below this native threshold, before world enumeration.
                const float threshold = rs::from_bits(0x37800000);
                const bool visible =
                    rs::abs(delta[0]) > threshold || rs::abs(delta[1]) > threshold || rs::abs(delta[2]) > threshold;
                if (visible) {
                    rs::Result<std::optional<F4>, E> hit = line(start, end, this->radius);
                    if (hit.is_err()) {
                        return rs::make_err(E(hit.unwrap_err()));
                    }
                    if (hit.unwrap().has_value()) {
                        return this->time_at_contact(*hit.unwrap(), time);
                    }
                }
                start = end;
            }
            const float fraction = vector_tracker::refined_reciprocal(this->duration) * time;
            const float step = rs::mul_add(end_step, fraction, (1.0f - fraction) * start_step);
            time += step;
            // The stock walk evaluates one step ahead and deliberately does
            // not clamp that point to duration. Do not replace with a uniform
            // sample count or an end-clamped parabola.
            end = this->evaluate(time + step);
        }
        return -1.0f;
    }

    F4 evaluate(float time) const;
    float step_size(float error_radius) const;
    float time_at_contact(F4 position, float time) const;
};
S3_DERIVE_EQ(TrajectoryQuery, position, velocity, gravity, duration, radius, start_error, end_error)

}  // namespace s3::camera
