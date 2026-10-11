// Port of skate-core/src/air/trajectory/selector.rs.
// Original selector Launch82D67848 and Update82D68800 with typed batch ownership.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/air/trajectory/grind.hpp"
#include "s3/core/air/trajectory/launch.hpp"
#include "s3/core/air/trajectory/math.hpp"
#include "s3/core/air/trajectory/prediction.hpp"
#include "s3/core/air/trajectory/scoring.hpp"
#include "s3/core/air/trajectory/types.hpp"

namespace s3::air::trajectory::selector {

using math::Vector;
using prediction::Prediction;
using prediction::QueryRequest;
using prediction::QueryResult;
using prediction::Trajectory;
using scoring::Candidate;
using types::LaunchInfo;
using types::SelectorInput;
using types::SelectorSettings;

/// The current authored world contains collision triangles and no grind edges.
/// Pass this only after querying that actual world's topology. It is not a
/// replacement for a grind result when a world contains grind primitives.
struct WorldWithoutGrindEdges {};

struct Selection {
    std::size_t candidate_index = 0;  // 9636
    Prediction prediction{};          // owning copy at1696, pointer1680
    Vector start_velocity{};          // 2640
    Vector landing_normal{};          // 2656
    Vector collision_velocity{};      // 2672
    Vector collision_position{};      // 2688
    Trajectory com_trajectory{};      // 2704
    std::uint32_t surface_category = 0;  // 9640
    bool wall_ride = false;              // 9661
    std::optional<grind::GrindTarget> grind;
};

class TrajectorySelector {
public:
    static TrajectorySelector new_() { return TrajectorySelector{}; }
    bool pending() const { return pending_; }
    bool valid() const { return valid_; }
    bool just_changed() const { return just_changed_; }
    bool all_predictions_missed() const { return all_predictions_missed_; }
    std::optional<Vector> suggested_normal() const { return suggested_normal_; }
    const Selection* selection() const { return selection_ ? &*selection_ : nullptr; }
    std::optional<std::size_t> selected_index() const { return selected_index_; }
    bool grind_locked_to_middle() const { return grind_locked_to_middle_; }
    std::optional<Vector> grind_normal() const { return grind_normal_; }
    const LaunchInfo* launch_info() const { return launch_info_ ? &*launch_info_ : nullptr; }
    /// `&[QueryRequest]` (empty when no batch).
    const std::vector<QueryRequest>& requests() const;
    std::optional<Vector> com_displacement() const;
    std::optional<Vector> local_com_position() const;
    std::optional<Vector> local_board_position() const;
    std::optional<Vector> board_position() const;
    std::optional<Vector> query_origin() const;
    /// Update82D68800 always clears9660 even if no batch is ready.
    bool update_without_completion() {
        just_changed_ = false;
        return valid_;
    }
    /// Ground's explicit cancellation writes9657 only; cached observations stay.
    void cancel_pending() { pending_ = false; }

    /// Reset82D67228 invalidates landing observations, while retaining the
    /// in-flight request9657, winning result1680 and selected index9636.
    void reset();

    rs::Result<bool, std::string> launch(LaunchInfo info, SelectorInput input, const SelectorSettings& s);

    /// Complete a real world query batch in order. A second pass is exposed by
    /// pending()/requests(); host scheduling preserves its next Update boundary.
    rs::Result<bool, std::string> complete_batch(const std::vector<QueryResult>& results, SelectorInput input,
                                                 const SelectorSettings& s, scoring::GrindFn grind,
                                                 scoring::LineFn line);

private:
    void launch_pass(SelectorInput input, const SelectorSettings& s);
    Trajectory com_trajectory(Selection selection, SelectorInput input, const SelectorSettings& s) const;

    std::optional<LaunchInfo> launch_info_;
    std::optional<launch::LaunchBatch> batch_;
    std::vector<Candidate> candidates_;
    std::optional<Selection> selection_;
    std::optional<std::size_t> selected_index_;
    bool grind_locked_to_middle_ = false;    // 9653
    std::optional<Vector> grind_normal_;     // 9656/2896
    std::uint16_t pass_ = 0;                 // 9632
    bool adjusted_on_vert_ = false;          // 9651
    bool pending_ = false;                   // 9657
    bool valid_ = false;                     // 9658
    bool just_changed_ = false;              // 9660
    bool all_predictions_missed_ = false;    // 9662
    std::optional<Vector> suggested_normal_;  // 9655/2880
};

}  // namespace s3::air::trajectory::selector
