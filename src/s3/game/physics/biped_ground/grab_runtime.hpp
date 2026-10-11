// Port of skate-game/src/physics/biped_ground/grab_runtime.rs
//! PlayerGrabSpline82D73FC0/74270/740F8/749D0, distinct from possession.
//! Scene execution is concrete; retained results become visible only at Sync.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/player/offboard/grab_scene.hpp"
#include "s3/core/player/offboard/ground_entry.hpp"
#include "s3/core/player/offboard/ground_query.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/biped_ground/grab_runtime/selection.hpp"

namespace s3::game::physics::offboard::grab_scene {
struct Scene;  // physics/offboard/grab_scene.hpp (Scene<'a>)
}

namespace s3::game::physics::biped_ground::grab_runtime {

using ::s3::player::offboard::grab_scene::closest_point;
using ::s3::player::offboard::grab_scene::Descriptor;
using ::s3::player::offboard::grab_scene::Hit;
using ::s3::player::offboard::grab_scene::Line;
using ::s3::player::offboard::grab_scene::Query;
using ::s3::player::offboard::grab_scene::Record;
using ::s3::player::offboard::ground_entry::Frame;
using ::s3::player::offboard::ground_entry::Vector;
using ::s3::player::offboard::ground_query::QueryContext;
using Scene = ::s3::game::physics::offboard::grab_scene::Scene;

///None for a record means clear its publication bit, retaining previous payload.
///Nested object Option distinguishes no completion from completed no-object.
struct Publication {
    std::array<std::optional<Record>, 2> records{};
    std::optional<std::optional<std::uint32_t>> object{};
};

/// #[derive(Default)]
struct Owner {
    std::vector<Query> queries{};
    std::optional<std::vector<Record>> query_result{};
    std::vector<Record> pending{};
    std::vector<Record> validated{};
    std::optional<std::vector<std::optional<Hit>>> validation{};
    std::array<std::optional<Descriptor>, 2> requests{};
    std::array<std::optional<Record>, 2> data{};
    std::array<bool, 2> data_ready{};
    std::optional<std::array<Line, 5>> interactable_request{};
    std::optional<std::optional<std::uint32_t>> interactable_result{};
    bool interactable_latched = false;
    std::uint8_t flags_12836 = 0;
    Vector query_position{};
    Vector validation_position{};

    void query(Query query);
    void request_primary(Descriptor descriptor);
    ///8275FD00 captures five actual forward lines; query execution is deferred.
    void request_interactable(Frame frame, QueryContext context);
    ///82760060: bounded queries, requested data, then interactable results.
    Status execute_queries(const Scene& scene);
    ///82D74270: old validation is consumed BEFORE new query results spawn lines.
    Status sync(const Scene& scene, QueryContext context);
    std::optional<Record> best(Vector position) const;
    ///82D749D0 cancels spline queries/validation and clears only native readiness.
    ///Scene lines have already completed; dropping them does not publish results.
    void invalidate();
    ///GroundEnter82D30B64 additionally clears interactable publication readiness.
    void enter_reset();
    ///82D740F8 consumes completion, not retained candidate existence.
    Publication publish();
};

///82D74740, raw747B8/747F4/7481C..74874. S2 has an extra outer hit-index
///increment at82DCC0FC; S3 does not. Do not group hits into chunks of three.
void validate(std::vector<Record>& records, const std::vector<std::optional<Hit>>& hits);

}  // namespace s3::game::physics::biped_ground::grab_runtime
