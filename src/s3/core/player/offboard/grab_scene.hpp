// Port of skate-core/src/player/offboard/grab_scene.rs (+ record.rs, registry.rs, query.rs,
// qualify.rs public items). Original S3 grab-scene records and execution, separate
// from pending queries. Ground owns request readiness, validation, selection and binding.
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "s3/core/player/offboard/ground_query.hpp"
#include "s3/core/player/offboard/ground_sync.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::offboard::grab_scene {

using Vector = std::array<float, 4>;
using Frame = std::array<Vector, 4>;
using ground_query::QueryContext;
using ground_sync::BoardLimits;
using ground_sync::Bounds;
/// Rust `&'static str` errors.
using Error = const char*;

struct Descriptor {
    std::uint32_t kind = 0;
    std::uint32_t id = 0;
};
S3_DERIVE_EQ(Descriptor, kind, id)

// ---- record.rs ----
struct Geometry {
    std::uint32_t id = 0;
    std::vector<Vector> points;
    std::vector<Vector> approach_vectors;
    std::uint32_t word_60 = 0;
};
S3_DERIVE_EQ(Geometry, id, points, approach_vectors, word_60)

struct RatesData {
    std::uint32_t identity = 0;
    Vector vector_48{};
    Vector transform_position_48{};
};
struct PartData {
    std::uint32_t identity = 0;
    std::optional<RatesData> rates;
    std::array<float, 10> coefficients_0_to_36{};
};
/// Actual first-part data. None is native assembly with no first part; the
/// native default coefficients apply only to that proven branch.
struct AssemblyData {
    std::uint32_t identity = 0;
    std::optional<PartData> first_part;
};
struct RecordInput {
    Descriptor descriptor{};
    std::shared_ptr<const Geometry> geometry;
    Frame frame{};
    Vector object_vector_128{};
    const AssemblyData* assembly = nullptr;
    std::uint32_t word_272 = 0;
};

/// Lossless native288-byte value. Word196 is a geometry identity, not readiness.
/// (Rust tuple struct `Record([u32; 72], Arc<Geometry>)`.)
struct Record {
    std::array<std::uint32_t, 72> _0{};
    std::shared_ptr<const Geometry> _1;

    Descriptor descriptor() const { return Descriptor{_0[47], _0[48]}; }
    std::array<Vector, 2> endpoints() const { return {vector(64), vector(80)}; }
    std::optional<std::uint32_t> assembly() const {
        return _0[52] != 0 ? std::optional<std::uint32_t>(_0[52]) : std::nullopt;
    }
    bool valid() const { return _0[49] != 0; }
    const std::vector<Vector>& points() const { return _1->points; }
    Vector vector(std::size_t offset) const {
        return rs::from_fn<4>([&](std::size_t i) { return rs::from_bits(_0[offset / 4 + i]); });
    }
    float scalar(std::size_t offset) const { return rs::from_bits(_0[offset / 4]); }
    std::uint8_t byte(std::size_t offset) const { return rs::to_be_bytes_u32(_0[offset / 4])[offset % 4]; }
    void set_vector(std::size_t offset, Vector value) {
        for (std::size_t i = 0; i < 4; ++i) _0[offset / 4 + i] = rs::to_bits(value[i]);
    }
    static rs::Result<Record, Error> from_geometry(const RecordInput& i);
};
inline bool operator==(const Record& a, const Record& b) {
    return a._0 == b._0 && (a._1 == b._1 || (a._1 && b._1 && *a._1 == *b._1));
}
inline bool operator!=(const Record& a, const Record& b) { return !(a == b); }

// ---- registry.rs ----
/// 82589130: selected record44/48, flag52 and matching assembly40.
struct ProviderDmo {
    std::uint8_t selection_variant = 0;
    std::int32_t matching_group = 0;
    bool record_enabled = false;
};
/// 82C4BE80: actual dynamic-object provider, no actor matching gate.
struct ProviderLivingWorld {};
struct Provider {
    std::variant<ProviderDmo, ProviderLivingWorld> value;
};
struct Spline {
    Descriptor descriptor{};
    std::shared_ptr<const Geometry> geometry;
    std::uint32_t word_272 = 0;
};
struct Object {
    std::uint32_t id = 0;
    Provider provider{};
    /// Real provider exclusion: Dmo record92 / LW virtual108.
    bool disabled = false;
    bool assembly_ready = false;
    std::optional<AssemblyData> assembly;
    Frame frame{};
    Vector object_vector_128{};
    std::vector<Spline> splines;

    rs::Result<Record, Error> record(const Spline& spline) const;
};

struct Hit {
    float fraction = 0.0f;
    std::optional<std::uint32_t> assembly;
};
S3_DERIVE_EQ(Hit, fraction, assembly)

struct Registry {
    std::vector<Object> objects;

    static rs::Result<Registry, Error> new_(std::vector<Object> objects);
    std::optional<std::uint32_t> eligible_object(Hit hit) const;
};

struct Query {
    Vector position{};
    /// 82760508 sorts at+96, distinct from provider position+80.
    Vector sort_position{};
    Bounds bounds{};
    BoardLimits limits{};
    std::uint32_t mode = 0;
    std::size_t capacity = 0;
    QueryContext context{};
};

struct Line {
    Vector start{};
    Vector end{};
    float radius = 0.0f;
    std::int32_t group = 0;
    std::uint32_t reject_flags = 0;
    /// Batch source pools: validation82D744F8 uses7; interactable8275FD00 uses2.
    std::uint8_t source_pool_mask = 0;
    /// Actor virtual56: NOT a collision-mesh rejection mask.
    std::uint32_t selection_flags = 0;
};

// ---- qualify.rs / query.rs ----
bool qualify(const Record& record, Vector position, Bounds bounds, BoardLimits limits);
/// 82E09D28 retains unnormalized direction below its native LENGTH threshold.
Vector closest_point(Vector position, const std::array<Vector, 2>& ab);
/// Select the nearest record FIRST, then return that record's eligibility.
/// An ineligible nearer record suppresses a farther eligible record.
std::optional<Record> best_spline(const std::vector<Record>& records, Vector position);
/// 82760508 mode4, actual provider enumeration, cap32 BEFORE distance sort.
rs::Result<std::vector<Record>, Error> query(const Registry& registry, const Query& q);

}  // namespace s3::player::offboard::grab_scene
