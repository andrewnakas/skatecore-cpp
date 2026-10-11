// Port of skate-core/src/player/offboard/ground_query.rs (+ consume/geometry/packet re-exports).
// Original TU3 Biped ground geometry:82D32808,82C20728,82C20C08,82D31620.
// Scene enumeration/collision is supplied explicitly by the canonical host.
// The host must consume the previous Sync query during the next PreUpdate.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::offboard::ground_query {

using s3::math::Vector3;

struct Frame {
    Vector3 right{};
    Vector3 up{};
    Vector3 forward{};
    Vector3 position{};
    static const Frame IDENTITY;
};
inline constexpr Frame Frame::IDENTITY = Frame{Vector3{1.0f, 0.0f, 0.0f}, Vector3{0.0f, 1.0f, 0.0f},
                                               Vector3{0.0f, 0.0f, 1.0f}, Vector3{0.0f, 0.0f, 0.0f}};
S3_DERIVE_EQ(Frame, right, up, forward, position)

struct QueryContext {
    std::uint32_t selection_flags_2948 = 0;
    std::int32_t matching_id_2952 = 0;
};
S3_DERIVE_EQ(QueryContext, selection_flags_2948, matching_id_2952)

struct Edge {
    Vector3 start{};
    Vector3 end{};
};
S3_DERIVE_EQ(Edge, start, end)

struct EdgeSearch {
    Vector3 min{};
    Vector3 max{};
    Frame frame{};
    QueryContext context{};
    bool narrow_forward = false;
};

struct EdgeSelection {
    Edge edge{};
    Vector3 closest{};
};

struct Line {
    Vector3 start{};
    Vector3 end{};
    float radius = 0.0f;
};
S3_DERIVE_EQ(Line, start, end, radius)

struct GroundQueryPacket {
    QueryContext context{};
    Vector3 center{};
    Vector3 up{};
    Vector3 tangent{};
    std::array<Line, 7> lines{};

    /// 82764AB0: Ground pool,21888 pool, and21892 only on island.flags==3.
    static constexpr std::uint8_t SOURCE_POOL_MASK = 7;
    /// 8276D510 adds no facing rejection; triangle winding rules still apply.
    static constexpr std::uint8_t FACING_FLAGS = 3;
    static constexpr std::uint32_t MESH_EXCLUSION_MASK = 0;
    static constexpr std::array<std::uint32_t, 4> MATERIAL_EXCLUSION_MASK{};
};

struct LineHit {
    Vector3 position{};
    Vector3 face_normal{};
    float fraction = 0.0f;
    std::uint16_t packed_surface = 0;
};
S3_DERIVE_EQ(LineHit, position, face_normal, fraction, packed_surface)

/// Complete fields read by82D31620; not the unrelated general grind classifier.
struct GroundGeometry {
    Frame frame{};
    std::uint32_t kind = 0;
    bool flag26 = false;
    bool flag27 = false;
    bool flag28 = false;
};

struct ConsumeInput {
    Frame frame_80{};
    Vector3 contact_position_192{};
    std::uint32_t contact_flags_368 = 0;
    float reach_364 = 0.0f;
    Vector3 previous_input_up_416{};
};

struct GroundAdjustment {
    bool state_752 = false;
    bool state_753 = false;
    bool state_754 = false;
    Frame frame_768{};
    Vector3 input_up_416{};
};

/// The runtime implements actual authored edge/scene-body queries, never cached
/// observations. (Rust trait with associated `Error` -> templated interface.)
template <class E>
struct GroundQueryScene {
    using Error = E;
    virtual ~GroundQueryScene() = default;
    virtual rs::Result<std::vector<Edge>, E> edge_candidates(const EdgeSearch& search) = 0;
    /// Return actual seven query results. Preserve source pool ordering, group
    /// matching, fatness0, decoded face normals and strict nearest-hit ties.
    virtual rs::Result<std::array<std::optional<LineHit>, 7>, E> query_lines(const GroundQueryPacket& packet) = 0;
};

// consume.rs
/// 82C20C08 fields directly consumed by82D31620; calls82C21558/82C1E170.
GroundGeometry interpret_hits(const GroundQueryPacket& packet, const std::array<std::optional<LineHit>, 7>& hits);
/// 82D31620 including its PreUpdate resets82D30EAC..82D30F0C. None means the
/// previous query was not pending; contact fallback still executes.
GroundAdjustment consume_geometry(const ConsumeInput& input, const std::optional<GroundGeometry>& geometry);
// geometry.rs
/// 82D32168..321C0 and82D32808. Inputs are the live Biped frame and state1040.
EdgeSearch edge_search(const Frame& frame, QueryContext context, Vector3 state_vector_1040,
                       std::uint32_t processed_2488);
/// 82E09D28: finite-segment closest point, including the tiny-edge retain branch.
Vector3 closest_point(Vector3 point, Edge edge);
/// 82D32808: input candidates MUST retain provider order and its shared cap40.
/// Both distances must improve; this is not a nearest-distance sort.
std::optional<EdgeSelection> select_edge(const EdgeSearch& search, const std::vector<Edge>& candidates);
// packet.rs
/// 82D321D0..32320 then82C20728: all seven Biped lines, in source order.
std::optional<GroundQueryPacket> prepare_packet(const Frame& frame, QueryContext context,
                                                const EdgeSelection& selected, float deck_half_wheelbase);

}  // namespace s3::player::offboard::ground_query
