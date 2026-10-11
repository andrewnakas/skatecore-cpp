// Port of skate-core/src/air/trajectory/selector.rs.
#include "s3/core/air/trajectory/selector.hpp"

#include <algorithm>

namespace s3::air::trajectory::selector {

using namespace trajectory::math;

const std::vector<QueryRequest>& TrajectorySelector::requests() const {
    static const std::vector<QueryRequest> empty;
    return batch_ ? batch_->requests : empty;
}
std::optional<Vector> TrajectorySelector::com_displacement() const {
    return batch_ ? std::optional<Vector>(batch_->com_displacement) : std::nullopt;
}
std::optional<Vector> TrajectorySelector::local_com_position() const {
    return batch_ ? std::optional<Vector>(batch_->local_com_position) : std::nullopt;
}
std::optional<Vector> TrajectorySelector::local_board_position() const {
    return batch_ ? std::optional<Vector>(batch_->local_board_position) : std::nullopt;
}
std::optional<Vector> TrajectorySelector::board_position() const {
    return batch_ ? std::optional<Vector>(batch_->board_position) : std::nullopt;
}
std::optional<Vector> TrajectorySelector::query_origin() const {
    return batch_ ? std::optional<Vector>(batch_->origin) : std::nullopt;
}

void TrajectorySelector::reset() {
    valid_ = false;
    suggested_normal_ = std::nullopt;
    grind_normal_ = std::nullopt;
    just_changed_ = false;
    all_predictions_missed_ = false;
    pass_ = 0;
    grind_locked_to_middle_ = false;
    if (selection_) {
        selection_->com_trajectory = Trajectory{ZERO, ZERO, ZERO, -1.0f};
    }
    // The reset's remaining slots are not represented by this selector's
    // player-owned, triangle-world observations.
}

rs::Result<bool, std::string> TrajectorySelector::launch(LaunchInfo info, SelectorInput input,
                                                         const SelectorSettings& s) {
    if (pending_) {
        return false;
    }
    if (info.trajectory_count == 0) {
        return rs::make_err(std::string("GetLaunchInfo supplied zero trajectory candidates"));
    }
    pass_ = 0;
    valid_ = false;
    // Launch invalidates9636 but retains the cached winning result and COM.
    selected_index_ = std::nullopt;
    grind_normal_ = std::nullopt;
    adjusted_on_vert_ = launch::adjust_velocity(info, input, s);
    launch_info_ = info;
    launch_pass(input, s);
    return true;
}

void TrajectorySelector::launch_pass(SelectorInput input, const SelectorSettings& s) {
    if (!launch_info_) S3_PANIC("Launch owns the packet before its batch");
    const LaunchInfo info = *launch_info_;
    all_predictions_missed_ = false;
    if (selection_) {
        selection_->wall_ride = false;
    }
    launch::LaunchBatch batch = launch::batch(info, input, s);
    // Only velocities are written by InitTrajectoryInfo. Retain the other
    // candidate fields across passes until scoring writes a valid result.
    const std::size_t new_len = std::max(candidates_.size(), batch.requests.size());
    while (candidates_.size() < new_len) {
        Candidate c{};
        c.prediction = Prediction{QueryResult::miss(), batch.requests[0]};
        c.start_velocity = ZERO;
        c.normal = ZERO;
        c.collision_velocity = ZERO;
        c.collision_position = ZERO;
        c.score = 0.0f;
        c.wall_score = 0.0f;
        c.wall_ride = false;
        c.grind = std::nullopt;
        candidates_.push_back(c);
    }
    for (std::size_t index = 0; index < batch.requests.size() && index < candidates_.size(); ++index) {
        Candidate& c = candidates_[index];
        c.start_velocity = batch.velocities[index];
        c.prediction = Prediction{QueryResult::miss(), batch.requests[index]};
    }
    batch_ = std::move(batch);
    pending_ = true;
}

rs::Result<bool, std::string> TrajectorySelector::complete_batch(const std::vector<QueryResult>& results,
                                                                 SelectorInput input, const SelectorSettings& s,
                                                                 scoring::GrindFn grind, scoring::LineFn line) {
    just_changed_ = false;
    if (!pending_) {
        return valid_;
    }
    const std::size_t count = requests().size();
    if (results.size() != count) {
        return rs::make_err(std::string("Trajectory completion count differs from submitted batch"));
    }
    just_changed_ = pass_ == 2;
    pending_ = false;
    pass_ = static_cast<std::uint16_t>(pass_ + 1);
    for (std::size_t i = 0; i < candidates_.size() && i < results.size(); ++i) {
        candidates_[i].prediction.result = results[i];
    }
    if (count > candidates_.size()) S3_PANIC("range end index out of range for slice");
    const rs::Result<bool, std::string> scored =
        scoring::score(physics::Slice<Candidate>(candidates_.data(), count), pass_, adjusted_on_vert_, input, s,
                       std::move(grind), std::move(line));
    if (scored.is_err()) return rs::make_err(scored.unwrap_err());
    all_predictions_missed_ = scored.unwrap();
    std::size_t index = 0;
    float best = -100000000.0f;
    for (std::size_t i = 0; i < count && i < candidates_.size(); ++i) {
        if (candidates_[i].score > best) {
            best = candidates_[i].score;
            index = i;
        }
    }
    const Candidate c = candidates_[index];
    for (std::size_t i = 0; i < count && i < candidates_.size(); ++i) {
        if (candidates_[i].grind) {
            grind_normal_ = candidates_[i].grind->vertical_normal;
            break;
        }
    }
    selected_index_ = index;
    // The copied COM trajectory is only recalculated on a valid-time result.
    const std::optional<Trajectory> previous_com =
        selection_ ? std::optional<Trajectory>(selection_->com_trajectory) : std::nullopt;
    Selection selection;
    selection.candidate_index = index;
    selection.prediction = c.prediction;
    selection.start_velocity = c.start_velocity;
    selection.landing_normal = c.normal;
    selection.collision_velocity = c.collision_velocity;
    selection.collision_position = c.collision_position;
    // Ctor82D66DC0 initializes2704/2720/2736=zero,2752=-1.
    selection.com_trajectory = previous_com.value_or(Trajectory{ZERO, ZERO, ZERO, -1.0f});
    selection.surface_category = selection_ ? selection_->surface_category : 0u;
    selection.wall_ride = c.wall_ride;
    selection.grind = c.grind;
    if (c.prediction.result.contact_time < s.minimum_valid_time) {
        suggested_normal_ = c.prediction.result.contact_normal;
        valid_ = false;
        just_changed_ = false;
        selection_ = selection;
        return false;
    }
    selection.surface_category = (c.prediction.result.surface >> 7) & 31u;
    selection.com_trajectory = com_trajectory(selection, input, s);
    // 82D68C80: only a winning centre acquisition locks the grind.
    grind_locked_to_middle_ = index == 0 && c.grind.has_value();
    const bool second_pass = pass_ == 1 && count >= 2 && !grind_locked_to_middle_ &&
                             !(angle_between(selection.landing_normal, input.ground_normal) <
                               s.minimum_normal_delta_second_pass);
    selection_ = selection;
    valid_ = true;
    if (second_pass) {
        if (!launch_info_) S3_PANIC("completed launch packet");
        LaunchInfo& info = *launch_info_;
        info.start_velocity = selection.start_velocity;
        info.cone_angle_x = s.cone_x_second_pass;
        info.cone_angle_z = s.cone_z_second_pass;
        launch_pass(input, s);
        pass_ = 2;
    }
    return valid_;
}

Trajectory TrajectorySelector::com_trajectory(Selection selection, SelectorInput input,
                                              const SelectorSettings& s) const {
    if (!launch_info_) S3_PANIC("completed launch packet");
    if (!batch_) S3_PANIC("completed query batch");
    const LaunchInfo info = *launch_info_;
    const launch::LaunchBatch& batch = *batch_;
    Trajectory trajectory = selection.prediction.request.trajectory;
    const float scalar = s.landing_com_scalar_vs_slope.evaluate(selection.landing_normal[1]);
    const Vector delta = sub(info.animation_com_position, trajectory.position);
    const float height =
        rs::abs(dot(input.reference_up, sub(batch.origin, info.animation_com_position))) * scalar;
    const Vector offset = scale(selection.landing_normal, height);
    if (selection.prediction.result.contact_frame > 0) {
        trajectory.position = add(trajectory.position, delta);
        launch::adjust_trajectory(trajectory, selection.prediction.result.contact_frame, sub(offset, delta),
                                  s.maximum_trajectory_adjust);
    }
    return trajectory;
}

}  // namespace s3::air::trajectory::selector
