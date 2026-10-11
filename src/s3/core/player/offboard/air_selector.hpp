// Port of skate-core/src/player/offboard/air_selector.rs (+ adjustment.rs,
// candidates.rs, completion.rs, ledge.rs, math.rs).
// Persistent offboard selector72: S3 82D6BD78..82D6EA6C.
// Separate from onboard Air. World work is submitted/staged by the game host;
// only explicit consume calls publish results. No physical solver ownership.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>

#include "s3/core/player/_deps/air_trajectory.hpp"
#include "s3/core/player/offboard/air_launch/packet.hpp"
#include "s3/core/player/offboard/air_selector/ledge.hpp"
#include "s3/core/player/offboard/air_selector/math.hpp"
#include "s3/core/player/offboard/biped_air/result.hpp"
#include "s3/core/player/offboard/biped_air/sampling.hpp"
#include "s3/core/player/offboard/biped_air/selection.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::player::offboard::air_selector {

namespace recovered = biped_air::recovered;
using air::trajectory::Prediction;
using air::trajectory::QueryRequest;
using air::trajectory::QueryResult;
using air::trajectory::Trajectory;
using air_launch::packet::Packet;
using point_graph::PointGraph;
using recovered::result::TrajectoryResult;
using recovered::sampling::SelectorState;
using recovered::selection::Candidate;
using Vector = std::array<float, 4>;
using Frame = std::array<Vector, 4>;

inline const float DT = rs::from_bits(0x3c888889u);
inline constexpr std::size_t MAX_CANDIDATES = 16;
inline constexpr std::uint32_t MESH_REJECT_MASK = 0x6000;

/// Original stock attributes, not incoming material policy.
struct Settings {
    /// AB0D9EAEBFC584E9
    float height = 0.0f;
    /// 620EEA2E27A371A2
    float sphere_radius = 0.0f;
    /// 1B6E3FF7EC72CDE1
    std::int32_t start_index = 0;
};

struct Context {
    std::uint32_t selection_flags_2948 = 0;
    std::int32_t matching_group_2952 = 0;
    Vector up_544{};
    Vector forward_224{};
};

struct Selector {
    SelectorState sampling{};
    Packet launch{};
    std::vector<Candidate> candidates;
    std::vector<Prediction> predictions;
    std::vector<float> scores;
    std::optional<std::size_t> selected_index;
    ///6080 is NOT the shifted/animation-adjusted sampling trajectory8144.
    Candidate selected_candidate{};
    Vector offset_8272{};
    Vector correction_8304{};
    Vector ledge_normal_8320{};
    bool ledge_selected_8496 = false;
    bool just_changed_8497 = false;
    bool requery_pending_8499 = false;
    std::int32_t requery_count_8488 = 0;
    Vector requery_position_8352{};
    Vector requery_normal_8368{};

    /// Rust `impl Default`: `Selector::new(Packet::initialized(0.))`.
    static Selector default_() { return new_(Packet::initialized(0.0f)); }
    static Selector new_(const Packet& launch);
    ///82D6BD78 retains packet3904. Reset does not invent a new launch.
    void reset();
    ///82D2EF20 clears only these two selector flags; retained predictions survive.
    void exit() {
        sampling.pending_8492 = false;
        sampling.preinitialized_8494 = false;
    }
    ///82D6CA58: once per launch. Invalid authored inputs fail.
    rs::Result<std::vector<QueryRequest>, const char*> begin_launch(const Packet& launch, Vector gravity,
                                                                    Settings settings);
    void sample(std::int32_t frame, float timestep, const PointGraph<8>& curve, TrajectoryResult& out) {
        sampling.sample(frame, timestep, curve, out);
    }
    ///82D6E798. Caller supplies current processed suppression flags.
    rs::Result<std::optional<QueryRequest>, const char*> begin_requery(std::uint32_t flags_2472,
                                                                       std::uint32_t flags_2488, float radius);

    // ---- adjustment.rs ----
    void adjust_animation(std::int32_t frame, Vector animation, const Frame& axes, float radius);

    // ---- completion.rs ----
    ///Populate native landing observations before candidate0's ledge stage.
    rs::Result<rs::Unit, const char*> observe_launch(const std::vector<QueryResult>& results);
    ///Native scoring uses the pre-ledge normal/displacement even if candidate0
    ///is subsequently adjusted. Caller passes the original observations.
    rs::Result<std::size_t, const char*> select_launch(const Context& context, const Candidate& original_first,
                                                       const std::optional<ledge::Adjustment>& ledge);
    ///82D6E8A0. A miss clears pending only.
    rs::Result<rs::Unit, const char*> complete_requery(const Prediction& prediction);
};

QueryRequest request(Trajectory trajectory, float radius);
rs::Result<rs::Unit, const char*> validate_request(const QueryRequest& q);

namespace candidates {
/// 82D6BF90 (pub(super)): (candidates, offset, initial correction).
rs::Result<std::tuple<std::vector<Candidate>, Vector, Vector>, const char*> prepare(const Packet& p, Vector gravity,
                                                                                    Settings s);
}  // namespace candidates

}  // namespace s3::player::offboard::air_selector
