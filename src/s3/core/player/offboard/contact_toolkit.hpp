// Port of skate-core/src/player/offboard/contact_toolkit.rs.
// OffboardGroundAnalyzer query ownership (TU3 82D81068/82D811C8).
//
// Submitted queries become visible only through the ordered analyzer refresh.
// Module-level primitives (Vector, Frame, UP, ZERO, IDENTITY, dot/sub/scale/cross/length,
// triangle_normal, LineHit) live in contact_toolkit/base.hpp.
#pragma once
#include <optional>
#include <utility>
#include <vector>

#include "s3/core/player/_deps/air_trajectory.hpp"
#include "s3/core/player/offboard/contact_toolkit/base.hpp"
#include "s3/core/player/offboard/contact_toolkit/candidate.hpp"
#include "s3/core/player/offboard/contact_toolkit/classification.hpp"
#include "s3/core/player/offboard/contact_toolkit/prefix.hpp"
#include "s3/core/player/offboard/contact_toolkit/probes.hpp"
#include "s3/core/player/offboard/contact_toolkit/samples.hpp"
#include "s3/core/player/offboard/contact_toolkit/sweep.hpp"

namespace s3::player::offboard::contact_toolkit {

/// Actual batch results, indexed in submission order, not sorted by distance.
struct QueryResults {
    std::array<air::trajectory::QueryResult, 3> trajectories{};
    std::vector<std::optional<LineHit>> lines;
    /// Authored static query edges in canonical traversal order, at most forty.
    std::vector<std::array<Vector, 2>> edges;
};

/// The scene owns geometry; the toolkit owns query lifetime and retained output.
/// (Rust trait with associated `Error`; `Owner::submit` accepts any type with a
/// matching `execute`, this base is a convenience.)
template <class E>
struct Scene {
    using Error = E;
    virtual ~Scene() = default;
    virtual rs::Result<QueryResults, E> execute(const Batch& batch) const = 0;
};

struct Collected {
    Batch batch;
    QueryResults results;
    ContactPrefix prefix;
    Samples samples;
};

/// Canonical lifetime of submission316, readiness320, and input0..96.
/// No executor or scene reference is retained across frames.
class Owner {
public:
    Owner() = default;

    /// Update1_ProcessInput82DB405C..4094 ages then completes the preceding
    /// batch before physical input is processed, regardless of active state.
    void begin_input() {
        readiness_ = readiness_ == 0 ? 0 : readiness_ - 1;
        (void)refresh();
    }
    /// Ground Reset82D30C20..40 / physical reset82DB93B0..CC.
    /// Complete outstanding work first; only readiness and classification
    /// history are cleared. The original does not erase the retained candidate.
    void reset_history() {
        (void)refresh();
        readiness_ = 0;
        history_ = classification::History{};
    }
    ContactPrefix prefix() const { return prefix_; }

    /// GroundSync82D32164 supplies the seven vectors after Skeleton Update.
    /// Host execution is synchronous, but visibility remains next-consume only.
    template <class S>
    rs::Result<rs::Unit, typename S::Error> submit(const Input& input, std::int32_t matching_group, const S& scene) {
        Batch batch = layout_.prepare(input, matching_group);
        auto results = scene.execute(batch);
        if (results.is_err()) return rs::make_err(results.unwrap_err());
        readiness_ = 0;
        pending_ = std::make_pair(std::move(batch), std::move(results.unwrap()));
        return rs::Unit{};
    }

    /// Consume the prior submission through every analyzer stage. Candidate and
    /// classification history persist; query observations are frame-local.
    std::optional<Collected> refresh();

    bool has_pending() const { return pending_.has_value(); }
    std::uint32_t readiness() const { return readiness_; }

private:
    ProbeLayout layout_ = ProbeLayout::stock();
    std::optional<std::pair<Batch, QueryResults>> pending_;
    std::uint32_t readiness_ = 0;
    ContactPrefix prefix_ = ContactPrefix::reset();
    candidate::Candidate candidate_{};
    classification::History history_{};
};

}  // namespace s3::player::offboard::contact_toolkit
