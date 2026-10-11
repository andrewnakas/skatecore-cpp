// Port of skate-game/src/grind_world/provider.rs.
//! Static stock assets only. Registration follows package source occurrence
//! order; native octree order is retained within each registered section.
#include "s3/game/grind_world/provider.hpp"

#include <cmath>
#include <map>
#include <string_view>

#include "s3/game/common.hpp"
#include "s3/game/grind_world/derive.hpp"

namespace s3::game::grind_world::provider {

namespace {

using ::s3::data::JsonDoc;
using ::s3::data::JsonKind;
using ::s3::data::JsonValue;

/// serde_json `value[key]` on a non-object or missing key is Null.
JsonValue index(const JsonValue& value, std::string_view key) {
    if (!value.valid() || !value.is_object()) return JsonValue();
    return value.get(key);
}
std::optional<std::string> as_str(const JsonValue& v) {
    if (!v.valid() || !v.is_string()) return std::nullopt;
    return std::string(v.text());
}
std::optional<std::uint64_t> as_u64(const JsonValue& v) {
    if (!v.valid() || !v.is_number()) return std::nullopt;
    auto r = v.to_u64();
    if (!r.ok) return std::nullopt;
    return r.value;
}
std::optional<bool> as_bool(const JsonValue& v) {
    if (!v.valid() || !v.is_bool()) return std::nullopt;
    return v.as_bool();
}

rs::Result<std::string, std::string> string(const JsonValue& value, const char* key) {
    auto s = as_str(index(value, key));
    if (!s) return rs::make_err(fmt::cat("WMET rail missing string ", key));
    return *s;
}
rs::Result<std::uint64_t, std::string> number(const JsonValue& value, const char* key) {
    auto n = as_u64(index(value, key));
    if (!n) return rs::make_err(fmt::cat("WMET rail missing unsigned ", key));
    return *n;
}
/// `u64::from_str_radix(s, 16)` (optional leading '+', at least one digit, no overflow).
std::optional<std::uint64_t> from_hex(std::string_view s) {
    if (!s.empty() && s[0] == '+') s.remove_prefix(1);
    if (s.empty()) return std::nullopt;
    std::uint64_t out = 0;
    for (const char c : s) {
        std::uint64_t d = 0;
        if (c >= '0' && c <= '9') d = std::uint64_t(c - '0');
        else if (c >= 'a' && c <= 'f') d = std::uint64_t(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = std::uint64_t(c - 'A' + 10);
        else return std::nullopt;
        if (out > (UINT64_MAX >> 4)) return std::nullopt;
        out = (out << 4) | d;
    }
    return out;
}
rs::Result<std::uint64_t, std::string> parse_id(const JsonValue& value, const char* key) {
    S3G_TRY(const std::string text, string(value, key));
    std::string_view v = text;
    if (v.substr(0, 2) == "0x") v.remove_prefix(2);
    auto id = from_hex(v);
    if (!id) return rs::make_err(fmt::cat("Invalid WMET ", key));
    return *id;
}

}  // namespace

rs::Result<StaticProvider, std::string> StaticProvider::new_(const SkateMap* map_ptr) {
    if (map_ptr == nullptr) {
        return authored({});
    }
    const SkateMap& map = *map_ptr;
    // Stock maps carry native rail payloads plus the WMET identity manifest.
    // Everything else is a custom map: its own rails, if any, are grinded
    // as authored, and edges are derived from its triangle collision.
    bool all_without_native = true;
    for (const auto& rail : map.rails) {
        if (rail.native.has_value()) {
            all_without_native = false;
            break;
        }
    }
    bool any_wmet = false;
    for (const auto& e : map.extensions) {
        if (e.tag_is("WMET")) {
            any_wmet = true;
            break;
        }
    }
    if (all_without_native || !any_wmet) {
        return custom(map);
    }
    const ::s3::data::Extension* extension = nullptr;
    std::size_t wmet_count = 0;
    for (const auto& e : map.extensions) {
        if (e.tag_is("WMET")) {
            if (extension == nullptr) extension = &e;
            ++wmet_count;
        }
    }
    if (extension == nullptr) {
        return rs::make_err(std::string("Stock grind provider requires WMET source identity metadata"));
    }
    if (extension->schema != 1 || wmet_count > 1) {
        return rs::make_err(std::string("Expected exactly one WMET schema 1 extension"));
    }
    auto parsed = JsonDoc::parse(std::string_view(reinterpret_cast<const char*>(extension->payload.data()),
                                                  extension->payload.size()));
    if (!parsed.ok) return rs::make_err(fmt::cat("Invalid WMET: ", parsed.error));
    const JsonDoc doc = std::move(parsed.value);
    const JsonValue manifest = doc.root();
    const auto mode = as_str(index(index(manifest, "grind_coordinate_policy"), "mode"));
    if (!mode || *mode != "world_space") {
        return rs::make_err(std::string("Static grind provider requires verified world_space spline payloads"));
    }
    const JsonValue records_v = index(manifest, "grind_splines");
    if (!records_v.valid() || !records_v.is_array()) {
        return rs::make_err(std::string("WMET grind_splines array missing"));
    }
    std::vector<JsonValue> records;
    for (JsonValue r : records_v) records.push_back(r);
    if (records.size() != map.rails.size()) {
        return rs::make_err(std::string("WMET/package rail count mismatch"));
    }
    S3G_TRY(const auto bytes, spline::build(&map));
    S3G_TRY(auto decoded, spline::decoded_from_blob(bytes));
    auto primitives = std::move(decoded.first);
    auto metadata = std::move(decoded.second);
    auto word = [&](std::size_t at) -> std::uint32_t {
        return (std::uint32_t(bytes[at]) << 24) | (std::uint32_t(bytes[at + 1]) << 16) |
               (std::uint32_t(bytes[at + 2]) << 8) | std::uint32_t(bytes[at + 3]);
    };
    std::vector<std::pair<SourceIdentity, std::vector<std::size_t>>> grouped;
    std::map<SourceIdentity, std::size_t> source_index;
    std::vector<Bounds> authored_bounds;
    authored_bounds.reserve(primitives.size());
    std::vector<Bounds> spatial_bounds;
    spatial_bounds.reserve(primitives.size());
    std::vector<std::size_t> source_for_primitive;
    source_for_primitive.reserve(primitives.size());
    std::vector<std::uint64_t> source_rail_indices;
    source_rail_indices.reserve(primitives.size());
    std::vector<std::array<std::uint64_t, 2>> rail_guids;
    rail_guids.reserve(map.rails.size());
    std::size_t ordinal = 0;
    for (std::size_t rail = 0; rail < records.size(); ++rail) {
        const JsonValue& record = records[rail];
        const auto& package_rail = map.rails[rail];
        if (!package_rail.native.has_value()) {
            return rs::make_err(fmt::cat("Stock rail ", package_rail.name, " lacks native cubic payload"));
        }
        SourceIdentity source;
        {
            S3G_TRY(source.stream_file, string(record, "stream_file"));
        }
        {
            S3G_TRY(source.asset_id, string(record, "asset_id"));
        }
        {
            S3G_TRY(source.section_index, number(record, "section_index"));
        }
        {
            S3G_TRY(source.section_offset, number(record, "section_offset"));
        }
        S3G_TRY(const std::uint64_t source_rail, number(record, "rail_index"));
        const std::string expected_name =
            fmt::cat(source.asset_id, "_", source.section_index, "_", source_rail);
        if (package_rail.name != expected_name) {
            return rs::make_err(fmt::cat("Stock rail provenance/name mismatch: ", package_rail.name));
        }
        const std::size_t header = 16 + rail * 32;
        const std::size_t first = std::size_t(word(header + 20));
        const std::size_t last = std::size_t(word(header + 24));
        const std::size_t count = (last - first) / 144 + 1;
        const std::uint64_t spline_id = (std::uint64_t(word(header)) << 32) | std::uint64_t(word(header + 4));
        const std::uint64_t type_signature =
            (std::uint64_t(word(header + 8)) << 32) | std::uint64_t(word(header + 12));
        rail_guids.push_back({spline_id, type_signature});
        // Rust `||` short-circuits left to right; each `?` may return early.
        bool mismatch = false;
        {
            S3G_TRY(const std::uint64_t v, parse_id(record, "spline_id"));
            mismatch = v != spline_id;
        }
        if (!mismatch) {
            S3G_TRY(const std::uint64_t v, number(record, "segment_count"));
            mismatch = v != std::uint64_t(count);
        }
        if (!mismatch) {
            S3G_TRY(const std::uint64_t v, parse_id(record, "type_signature"));
            mismatch = v != type_signature;
        }
        if (!mismatch) {
            S3G_TRY(const std::uint64_t v, number(record, "flags"));
            mismatch = v != std::uint64_t(word(header + 16));
        }
        if (!mismatch) {
            S3G_TRY(const std::uint64_t v, number(record, "trailing_word"));
            mismatch = v != std::uint64_t(word(header + 28));
        }
        if (!mismatch) {
            const auto closed = as_bool(index(record, "closed"));
            mismatch = !(closed.has_value() && *closed == package_rail.closed);
        }
        if (mismatch) {
            return rs::make_err(fmt::cat("Stock rail native identity/count mismatch: ", package_rail.name));
        }
        std::size_t asset = 0;
        const auto found = source_index.find(source);
        if (found != source_index.end()) {
            asset = found->second;
        } else {
            asset = grouped.size();
            source_index.emplace(source, asset);
            grouped.emplace_back(source, std::vector<std::size_t>{});
        }
        for (std::size_t segment = 0; segment < count; ++segment) {
            const std::size_t at = first + 144 * segment;
            Bounds bounds;
            for (std::size_t i = 0; i < 3; ++i) bounds.min[i] = rs::from_bits(word(at + 80 + i * 4));
            for (std::size_t i = 0; i < 3; ++i) bounds.max[i] = rs::from_bits(word(at + 96 + i * 4));
            authored_bounds.push_back(bounds);
            spatial_bounds.push_back(bounds.identity_transformed());
            grouped[asset].second.push_back(ordinal);
            source_for_primitive.push_back(asset);
            source_rail_indices.push_back(source_rail);
            ordinal += 1;
        }
    }
    std::vector<Asset> assets;
    for (auto& [source, indices] : grouped) {
        if (indices.empty()) {
            return rs::make_err(std::string("Empty stock grind source section"));
        }
        Bounds acc = spatial_bounds[indices[0]];
        for (std::size_t k = 1; k < indices.size(); ++k) acc = acc.union_(spatial_bounds[indices[k]]);
        const Bounds bounds = acc.padded();
        std::vector<Bounds> entries;
        entries.reserve(indices.size());
        for (const std::size_t i : indices) entries.push_back(spatial_bounds[i]);
        S3G_TRY(Octree tree, Octree::new_(bounds, std::move(entries)));
        assets.push_back(Asset{std::move(source), bounds, std::move(indices), std::move(tree)});
    }
    StaticProvider out;
    out.primitives_ = std::move(primitives);
    out.metadata_ = std::move(metadata);
    out.rail_guids = std::move(rail_guids);
    out.assets = std::move(assets);
    out.authored_bounds = std::move(authored_bounds);
    out.source_for_primitive = std::move(source_for_primitive);
    out.source_rail_indices = std::move(source_rail_indices);
    return out;
}

rs::Result<StaticProvider, std::string> StaticProvider::custom(const SkateMap& map) {
    std::vector<std::vector<std::array<float, 3>>> own;
    own.reserve(map.rails.size());
    for (const auto& rail : map.rails) {
        std::vector<std::array<float, 3>> points = rail.points;
        // `points.first() != points.last()` compares Options (both None when empty).
        if (rail.closed && !points.empty() && points.front() != points.back()) {
            points.push_back(points.front());
        }
        own.push_back(std::move(points));
    }
    std::vector<std::array<std::array<float, 3>, 3>> triangles;
    triangles.reserve(map.geometry.collision.size());
    for (const auto& t : map.geometry.collision) triangles.push_back(t.points);
    // Without triangles nothing can be judged; the map's rails stand as authored.
    derive::Derived derived;
    if (triangles.empty()) {
        derived.own_blocked.assign(own.size(), false);
    } else {
        derived = derive::rails(triangles, own);
    }
    std::size_t too_wide = 0;
    for (const bool b : derived.own_blocked) {
        if (b) ++too_wide;
    }
    log::eprintln(fmt::cat("SKATE_GRIND_DERIVED map_rails=", map.rails.size(), " too_wide=", too_wide,
                           " derived=", derived.rails.size(), " triangles=", triangles.size()));
    std::vector<::s3::data::Rail> rails;
    const std::size_t n = std::min(map.rails.size(), derived.own_blocked.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto& rail = map.rails[i];
        if (rail.native.has_value() || !derived.own_blocked[i]) {
            ::s3::data::Rail r;
            r.name = rail.name;
            r.closed = rail.closed;
            r.points = rail.points;
            r.native = rail.native;
            rails.push_back(std::move(r));
        }
    }
    for (std::size_t i = 0; i < derived.rails.size(); ++i) {
        ::s3::data::Rail r;
        r.name = fmt::cat("derived_edge_", i);
        r.closed = false;
        r.points = std::move(derived.rails[i]);
        r.native = std::nullopt;
        rails.push_back(std::move(r));
    }
    return authored(rails);
}

rs::Result<StaticProvider, std::string> StaticProvider::authored(const std::vector<::s3::data::Rail>& rails) {
    S3G_TRY(const auto bytes, spline::build_rails(rails));
    S3G_TRY(auto decoded, spline::decoded_from_blob(bytes));
    auto primitives = std::move(decoded.first);
    auto metadata = std::move(decoded.second);
    auto be64 = [&](std::size_t at) {
        std::uint64_t v = 0;
        for (std::size_t i = 0; i < 8; ++i) v = (v << 8) | std::uint64_t(bytes[at + i]);
        return v;
    };
    std::vector<std::array<std::uint64_t, 2>> rail_guids;
    for (std::size_t i = 0; i < rails.size(); ++i) {
        const std::size_t at = 16 + i * 32;
        rail_guids.push_back({be64(at), be64(at + 8)});
    }
    std::vector<Bounds> authored_bounds;
    authored_bounds.reserve(primitives.size());
    for (const auto& p : primitives) {
        Bounds b;
        for (std::size_t i = 0; i < 3; ++i) b.min[i] = rs::min(p.start[i], p.end[i]);
        for (std::size_t i = 0; i < 3; ++i) b.max[i] = rs::max(p.start[i], p.end[i]);
        authored_bounds.push_back(b);
    }
    std::vector<Asset> assets;
    if (!authored_bounds.empty()) {
        Bounds acc = authored_bounds[0];
        for (std::size_t k = 1; k < authored_bounds.size(); ++k) acc = acc.union_(authored_bounds[k]);
        const Bounds bounds = acc.padded();
        std::vector<std::size_t> indices(primitives.size());
        for (std::size_t i = 0; i < indices.size(); ++i) indices[i] = i;
        S3G_TRY(Octree tree, Octree::new_(bounds, authored_bounds));
        assets.push_back(Asset{SourceIdentity{std::string(), "host-authored", 0, 0}, bounds, std::move(indices),
                               std::move(tree)});
    }
    std::vector<std::uint64_t> source_rail_indices;
    source_rail_indices.reserve(primitives.size());
    for (const auto& p : primitives) source_rail_indices.push_back(p.owner - 1);
    StaticProvider out;
    out.source_for_primitive.assign(primitives.size(), 0);
    out.primitives_ = std::move(primitives);
    out.metadata_ = std::move(metadata);
    out.rail_guids = std::move(rail_guids);
    out.assets = std::move(assets);
    out.authored_bounds = std::move(authored_bounds);
    out.source_rail_indices = std::move(source_rail_indices);
    return out;
}

std::optional<std::array<std::uint64_t, 2>> StaticProvider::spline_guids(std::uint64_t owner) const {
    if (owner == 0) return std::nullopt;
    const std::uint64_t rail = owner - 1;
    if (rail >= rail_guids.size()) return std::nullopt;
    return rail_guids[std::size_t(rail)];
}

std::optional<std::uint64_t> StaticProvider::source_rail_index(std::size_t primitive) const {
    if (primitive >= source_rail_indices.size()) return std::nullopt;
    return source_rail_indices[primitive];
}

const SourceIdentity* StaticProvider::source(std::size_t primitive) const {
    if (primitive >= source_for_primitive.size()) return nullptr;
    return &assets[source_for_primitive[primitive]].source;
}

std::optional<std::pair<std::array<float, 3>, std::array<float, 3>>> StaticProvider::bounds(
    std::size_t primitive) const {
    if (primitive >= authored_bounds.size()) return std::nullopt;
    return std::make_pair(authored_bounds[primitive].min, authored_bounds[primitive].max);
}

rs::Result<std::vector<std::size_t>, std::string> StaticProvider::query(std::array<float, 3> min,
                                                                       std::array<float, 3> max) const {
    for (std::size_t i = 0; i < 3; ++i) {
        if (!std::isfinite(min[i]) || !std::isfinite(max[i]) || min[i] > max[i]) {
            return rs::make_err(std::string("Invalid grind query bounds"));
        }
    }
    const Bounds query{min, max};
    std::array<float, 3> delta{};
    for (std::size_t i = 0; i < 3; ++i) delta[i] = min[i] - max[i];
    const float square = (delta[0] * delta[0] + delta[1] * delta[1]) + delta[2] * delta[2];
    if (!(square > rs::from_bits(0x37800000u))) {
        return std::vector<std::size_t>{};
    }
    std::vector<std::size_t> result;
    for (const Asset& asset : assets) {
        if (!asset.bounds.overlaps(query)) {
            continue;
        }
        for (const std::size_t local : asset.tree.query(query, 40 - result.size())) {
            result.push_back(asset.indices[local]);
        }
        if (result.size() == 40) {
            break;
        }
    }
    return result;
}

}  // namespace s3::game::grind_world::provider
