// Port of skate-core/src/player/offboard/biped_air/sampling.rs and selection.rs.
#include "s3/core/player/offboard/biped_air/sampling.hpp"

#include "s3/core/player/offboard/biped_air/selection.hpp"

namespace s3::player::offboard::biped_air::recovered {

static const float DT_ = rs::from_bits(0x3c888889u);

namespace sampling {

void SelectorState::sample(std::int32_t frame, float timestep, const point_graph::PointGraph<8>& blend,
                           TrajectoryResult& out) {
    frame_8484 = frame;
    const float time = static_cast<float>(frame) * DT_;
    const Selection s = selection;
    if (pending_8492) {
        out.position_272 = fallback_8208.position_at(time);
        out.velocity_288 = fallback_8208.velocity_at(time);
        out.valid_404 = false;
        out.contact_position_336 = Vector{};
        out.normal_304 = Vector{0.0f, 1.0f, 0.0f, 0.0f};
        out.contact_velocity_320 = Vector{};
    } else {
        elapsed_8388 += timestep;
        blend_8384 = blend.evaluate(elapsed_8388);
        const Vector first = s.trajectory_8144.position_at(time);
        const Vector second = fallback_8208.position_at(time);
        const float weight = blend_8384;
        out.position_272 =
            rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(second[i], 1.0f - weight, first[i] * weight); });
        // The velocity is the selected trajectory velocity, NOT the
        // derivative of the position blend and NOT the launch velocity.
        out.velocity_288 = s.trajectory_8144.velocity_at(time);
        out.valid_404 = s.valid_6200;
        out.contact_position_336 = s.position_6176;
        out.normal_304 = s.normal_6144;
        out.contact_velocity_320 = s.velocity_6160;
    }
    out.time_remaining_384 =
        s.result_present_3888
            ? static_cast<float>(static_cast<std::int32_t>(static_cast<std::uint32_t>(s.landing_frame_8480) -
                                                           static_cast<std::uint32_t>(frame))) *
                  DT_
            : 0.0f;
    out.duration_388 = s.scalar_8392;
    out.frame_400 = frame;
    out.adjustment_352 = adjustment_8336;
    const auto apex = s.trajectory_8144.highest_position();
    out.apex_368 = apex.first;
    out.apex_time_396 = apex.second;
    out.word_408 = s.word_8396;
    out.scalar_392 = s.candidate_scalar_100;
}

// ---- selection.rs ----
namespace {
void shift(Trajectory& trajectory, float time) {
    const Vector position = trajectory.position_at(time);
    const Vector velocity = trajectory.velocity_at(time);
    trajectory.position = position;
    trajectory.velocity = velocity;
}
}  // namespace

SelectorState SelectorState::reset_sampling(float retained_launch_scalar_100) {
    SelectorState s;
    s.pending_8492 = false;
    s.restart_allowed_8493 = true;
    s.preinitialized_8494 = false;
    s.fallback_8208 = selection::cleared_trajectory();
    s.selection.trajectory_8144 = selection::cleared_trajectory();
    s.selection.normal_6144 = Vector{};
    s.selection.velocity_6160 = Vector{};
    s.selection.position_6176 = Vector{};
    s.selection.valid_6200 = false;
    s.selection.result_present_3888 = false;
    s.selection.landing_frame_8480 = 1000;
    s.selection.scalar_8392 = 0.0f;
    s.selection.word_8396 = 0;
    s.selection.candidate_scalar_100 = retained_launch_scalar_100;
    s.adjustment_8336 = Vector{};
    s.blend_8384 = 0.0f;
    s.elapsed_8388 = 0.0f;
    s.frame_8484 = 0;
    return s;
}

void SelectorState::seed_fallback(Vector position, Vector velocity, Vector gravity) {
    fallback_8208 = Trajectory{position, velocity, gravity, rs::from_bits(0x40000000u)};  // 82060C50=2 seconds.
    pending_8492 = true;
}

bool SelectorState::commit(const selection::Candidate& candidate, Prediction& prediction, Vector offset_8272) {
    pending_8492 = false;
    const float time =
        static_cast<float>(static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(candidate.start_frame_112))) *
        DT_;
    if (prediction.result.valid()) {
        prediction.result.contact_frame = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(prediction.result.contact_frame) +
            static_cast<std::uint32_t>(candidate.start_frame_112));
        prediction.result.contact_time -= time;
    }
    // The query trajectory is shifted back but does not receive body offset.
    shift(prediction.request.trajectory, time);
    Trajectory trajectory = candidate.trajectory;
    shift(trajectory, time);
    trajectory.position = rs::from_fn<4>([&](std::size_t i) { return trajectory.position[i] + (-offset_8272[i]); });
    float duration = selection.scalar_8392;
    std::uint32_t word = selection.word_8396;
    const bool contact = prediction.result.valid();
    if (contact) {
        duration = static_cast<float>(candidate.landing_frame_116) * DT_;
        word = candidate.special_121 ? 2u : (prediction.result.surface >> 7) & 0x1fu;
    }
    selection.trajectory_8144 = trajectory;
    selection.normal_6144 = candidate.normal_64;
    selection.velocity_6160 = candidate.contact_velocity_80;
    selection.position_6176 = candidate.contact_position_96;
    selection.valid_6200 = candidate.valid_120;
    selection.result_present_3888 = true;
    selection.landing_frame_8480 = candidate.landing_frame_116;
    selection.scalar_8392 = duration;
    selection.word_8396 = word;
    selection.candidate_scalar_100 = candidate.contact_position_96[1];
    preinitialized_8494 = true;
    return contact;
}

}  // namespace sampling

namespace selection {

Trajectory cleared_trajectory() {
    return Trajectory{Vector{}, Vector{}, Vector{}, -1.0f};
}

std::optional<std::size_t> selected_index(const float* scores, std::size_t count) {
    if (count == 0) {
        return std::nullopt;
    }
    float best = rs::from_bits(0xccbebc20u);  // Original822F9420, -100000000.
    std::size_t index = 0;
    for (std::size_t candidate = 0; candidate < count; ++candidate) {
        const float score = scores[candidate];
        if (score > best) {
            best = score;
            index = candidate;
        }
    }
    return index;
}

}  // namespace selection

}  // namespace s3::player::offboard::biped_air::recovered
