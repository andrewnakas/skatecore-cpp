// Port of skate-game/src/grind_world/provider.rs.
//! Static stock assets only. Registration follows package source occurrence
//! order; native octree order is retained within each registered section.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "s3/core/physics/grind_contact.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/json.hpp"
#include "s3/data/skate_map.hpp"
#include "s3/game/grind_world/octree.hpp"
#include "s3/game/grind_world/spline.hpp"

namespace s3::game::grind_world::provider {

namespace rs = ::s3::rs;
using octree::Bounds;
using octree::Octree;
using ::s3::data::SkateMap;
using ::s3::physics::grind_contact::Primitive;

struct SourceIdentity {
    std::string stream_file;
    std::string asset_id;
    std::uint64_t section_index = 0;
    std::uint64_t section_offset = 0;
};
S3_DERIVE_EQ(SourceIdentity, stream_file, asset_id, section_index, section_offset)
/// Ordering for the `HashMap<SourceIdentity, usize>` lookup (lookup only; order never observed).
[[maybe_unused]] inline bool operator<(const SourceIdentity& a, const SourceIdentity& b) {
    if (a.stream_file != b.stream_file) return a.stream_file < b.stream_file;
    if (a.asset_id != b.asset_id) return a.asset_id < b.asset_id;
    if (a.section_index != b.section_index) return a.section_index < b.section_index;
    return a.section_offset < b.section_offset;
}

struct Asset {
    SourceIdentity source;
    Bounds bounds;
    std::vector<std::size_t> indices;
    Octree tree;
};

struct StaticProvider {
    // Private in Rust.
    std::vector<Primitive> primitives_;  // `primitives` (name clashes with the accessor)
    std::vector<spline::PrimitiveMetadata> metadata_;  // `metadata` (name clashes with the accessor)
    std::vector<std::array<std::uint64_t, 2>> rail_guids;
    std::vector<Asset> assets;
    /// Parallel to primitives; no endpoint-derived replacement boxes.
    std::vector<Bounds> authored_bounds;
    std::vector<std::size_t> source_for_primitive;
    std::vector<std::uint64_t> source_rail_indices;

    /// WMET preserves source section identities even when spline IDs repeat.
    /// Missing provenance is an explicit conversion prerequisite, not a ray miss.
    /// (`map: Option<&SkateMap>` -> nullable pointer.)
    static rs::Result<StaticProvider, std::string> new_(const SkateMap* map);
    /// The map's own rails plus edges found in its collision triangles. Maps
    /// whose collision is an embedded RWCM archive get no derived edges.
    static rs::Result<StaticProvider, std::string> custom(const SkateMap& map);
    /// Explicit host-authored polylines have no retail source section identity.
    static rs::Result<StaticProvider, std::string> authored(const std::vector<::s3::data::Rail>& rails);

    const std::vector<Primitive>& primitives() const { return primitives_; }
    const spline::PrimitiveMetadata* metadata(std::size_t primitive) const {
        return primitive < metadata_.size() ? &metadata_[primitive] : nullptr;
    }
    /// Resolve contact's map-local header handle without confusing it with GUID.
    std::optional<std::array<std::uint64_t, 2>> spline_guids(std::uint64_t owner) const;
    std::optional<std::uint64_t> source_rail_index(std::size_t primitive) const;
    const SourceIdentity* source(std::size_t primitive) const;
    std::optional<std::pair<std::array<float, 3>, std::array<float, 3>>> bounds(std::size_t primitive) const;
    /// S3 82C1EAD8 static pass only: query gate, ordered asset overlap and
    /// native per-asset octree traversal, at most forty original vector indices.
    /// Does not implement or pretend to query unavailable moving providers.
    rs::Result<std::vector<std::size_t>, std::string> query(std::array<float, 3> min, std::array<float, 3> max) const;
};

}  // namespace s3::game::grind_world::provider
