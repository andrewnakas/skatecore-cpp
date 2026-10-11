// Port of skate-core/src/player/offboard/grab_scene.rs and submodules
// (math, qualify{,/math,/polyline,/selection}, query, record, registry).
#include "s3/core/player/offboard/grab_scene.hpp"

#include "s3/core/player/offboard/grab_scene/math.hpp"
#include "s3/core/player/offboard/grab_scene/qualify/math.hpp"
#include "s3/core/player/offboard/grab_scene/qualify/polyline.hpp"

namespace s3::player::offboard::grab_scene {

// ---- record.rs ----
rs::Result<Record, Error> Record::from_geometry(const RecordInput& i) {
    using namespace grab_scene::math;
    const Geometry& g = *i.geometry;
    if (g.id == 0 || g.points.empty() || g.points.size() > 255 || g.approach_vectors.size() > 255) {
        return rs::make_err(Error("Grab geometry needs a real identity and native byte-sized point counts"));
    }
    auto nonfinite = [](const Vector& v) {
        for (float x : v)
            if (!rs::is_finite(x)) return true;
        return false;
    };
    bool bad = false;
    for (const auto& v : g.points) bad = bad || nonfinite(v);
    for (const auto& v : g.approach_vectors) bad = bad || nonfinite(v);
    for (const auto& v : i.frame) bad = bad || nonfinite(v);
    bad = bad || nonfinite(i.object_vector_128);
    if (bad) return rs::make_err(Error("Nonfinite authored grab geometry/frame"));
    Record r{{}, i.geometry};
    for (std::size_t j = 0; j < 4; ++j) r.set_vector(j * 16, i.frame[j]);
    r._0[47] = i.descriptor.kind;
    r._0[48] = i.descriptor.id;
    r._0[49] = r._1->id;
    r._0[50] = 0x80000000u;
    r._0[51] = r._1->word_60;
    r._0[68] = i.word_272;
    const Vector start = point(i.frame, r._1->points.front());
    const Vector end = point(i.frame, r._1->points.back());
    const Vector approach = r._1->approach_vectors.empty() ? Vector{0.0f, 1.0f, 0.0f, 0.0f} : r._1->approach_vectors.front();
    r.set_vector(64, start);
    r.set_vector(80, end);
    r.set_vector(96, direction(i.frame, approach));
    r.set_vector(128, i.object_vector_128);
    r.set_vector(224, Vector{1.0f, 1.0f, 1.0f, 1.0f});
    const std::pair<std::size_t, float> scalars[4] = {{240, 1.0f}, {244, 1.0f}, {248, rs::f32::MAX}, {252, rs::f32::MAX}};
    for (const auto& [offset, value] : scalars) r._0[offset / 4] = rs::to_bits(value);
    if (i.assembly) {
        const AssemblyData& a = *i.assembly;
        if (a.identity == 0) return rs::make_err(Error("Zero assembly identity is reserved for native null"));
        r._0[52] = a.identity;
        if (a.first_part) {
            const PartData& p = *a.first_part;
            if (p.identity == 0) return rs::make_err(Error("Zero part identity is reserved for native null"));
            r._0[53] = p.identity;
            for (std::size_t j = 0; j < 10; ++j) r._0[56 + j] = rs::to_bits(p.coefficients_0_to_36[j]);
            if (p.rates) {
                const RatesData& rates = *p.rates;
                if (rates.identity == 0) return rs::make_err(Error("Zero rates identity is reserved for native null"));
                r._0[54] = rates.identity;
                r._0[50] |= 0x40000000u;
                r.set_vector(144, rates.vector_48);
                r.set_vector(160, rates.transform_position_48);
            }
        }
    }
    const Vector delta = sub(end, start);
    const float straight = length(delta);
    const float inv = reciprocal(straight);
    r.set_vector(112, rs::map(delta, [&](float v) { return v * inv; }));
    r._0[45] = rs::to_bits(straight * 0.5f);
    float total = 0.0f;
    for (std::size_t k = 0; k + 1 < r._1->points.size(); ++k) {
        total += length(sub(r._1->points[k + 1], r._1->points[k]));
    }
    r._0[44] = rs::to_bits(total);
    return r;
}

// ---- registry.rs ----
rs::Result<Record, Error> Object::record(const Spline& spline) const {
    return Record::from_geometry(RecordInput{spline.descriptor, spline.geometry, frame, object_vector_128,
                                             assembly ? &*assembly : nullptr, spline.word_272});
}

rs::Result<Registry, Error> Registry::new_(std::vector<Object> objects) {
    for (std::size_t index = 0; index < objects.size(); ++index) {
        const Object& object = objects[index];
        if (object.id == 0) return rs::make_err(Error("Interactable object ID zero is reserved for none"));
        if (const auto* dmo = std::get_if<ProviderDmo>(&object.provider.value)) {
            if (dmo->selection_variant > 1) return rs::make_err(Error("Dmo body selection is a single bit"));
        }
        for (const auto& spline : object.splines) {
            if (!(spline.descriptor.kind == 1 || spline.descriptor.kind == 2)) {
                return rs::make_err(Error("Unknown native grab descriptor kind"));
            }
            auto rec = object.record(spline);
            if (rec.is_err()) return rs::make_err(rec.unwrap_err());
        }
        for (std::size_t k = 0; k < index; ++k) {
            if (objects[k].id == object.id) return rs::make_err(Error("Duplicate authored interactable object ID"));
        }
    }
    return Registry{std::move(objects)};
}

std::optional<std::uint32_t> Registry::eligible_object(Hit hit) const {
    // 82760368 rejects zero distance and missing assembly BEFORE lookup.
    if (hit.fraction == 0.0f) return std::nullopt;
    if (!hit.assembly) return std::nullopt;
    const std::uint32_t assembly = *hit.assembly;
    for (const auto& object : objects) {
        if (!object.disabled && object.assembly && object.assembly->identity == assembly) return object.id;
    }
    return std::nullopt;
}

// ---- qualify/polyline.rs ----
namespace qualify_::polyline {
namespace {
using namespace qualify_::math;
std::array<Vector, 4> frame(const Record& r) { return {r.vector(0), r.vector(16), r.vector(32), r.vector(48)}; }
std::vector<std::size_t> indices(const Record& r) {
    const std::size_t count = r.points().size();
    const bool reverse = (r.byte(200) & 0x20) != 0;
    std::vector<std::size_t> out(count);
    for (std::size_t i = 0; i < count; ++i) out[i] = reverse ? count - 1 - i : i;
    return out;
}
}  // namespace

float nearest_distance(const Record& r, Vector world) {
    if (r.points().size() <= 1) return 0.0f;
    const Vector local = inverse_point(frame(r), world);
    const auto idx = indices(r);
    Vector a = r.points()[idx[0]];
    float accumulated = 0.0f;
    float best = rs::f32::MAX;
    float distance = 0.0f;
    for (std::size_t k = 1; k < idx.size(); ++k) {
        const Vector b = r.points()[idx[k]];
        const Vector near = closest_point(local, {a, b});
        const Vector error = sub(local, near);
        const float squared = dot(error, error);
        if (best > squared) {
            best = squared;
            const Vector partial = sub(near, a);
            // 82D2D204 zero mask applies AFTER adding accumulated distance.
            distance = dot(partial, partial) == 0.0f ? 0.0f : accumulated + length(partial);
        }
        const Vector segment = sub(b, a);
        // 82D2D260 has the same post-add zero selection.
        accumulated = dot(segment, segment) == 0.0f ? 0.0f : accumulated + length(segment);
        a = b;
    }
    return distance;
}

Vector at_distance(const Record& r, float distance) {
    const auto endpoints = r.endpoints();
    if (r.points().size() <= 1 || 0.0f >= distance) return endpoints[0];
    const auto idx = indices(r);
    Vector a = r.points()[idx[0]];
    float accumulated = 0.0f;
    for (std::size_t k = 1; k < idx.size(); ++k) {
        const Vector b = r.points()[idx[k]];
        const Vector dir = sub(b, a);
        const float len = length(dir);
        accumulated += len;
        if (accumulated > distance) {
            if (0.0f >= len) return transform(frame(r), b);
            const float remaining = reciprocal(len) * (accumulated - distance);
            const Vector local = rs::from_fn<4>([&](std::size_t i) { return b[i] - dir[i] * remaining; });
            return transform(frame(r), local);
        }
        a = b;
    }
    return endpoints[1];
}
}  // namespace qualify_::polyline

// ---- qualify.rs ----
Vector closest_point(Vector position, const std::array<Vector, 2>& ab) {
    return qualify_::math::closest_point(position, ab);
}

bool qualify(const Record& record, Vector position, Bounds bounds, BoardLimits limits) {
    using namespace qualify_::math;
    namespace polyline = qualify_::polyline;
    const float length = record.scalar(176);
    float margin = limits.margin;
    if (margin > length * 0.5f) margin = length * 0.5f;
    const float distance = rs::min(rs::max(polyline::nearest_distance(record, position), margin), length - margin);
    const Vector point = polyline::at_distance(record, distance);
    const auto endpoints = record.endpoints();
    const Vector direction = sub(endpoints[0], endpoints[1]);
    const Vector forward = bounds.frame[2];
    const float facing = rs::abs(dot(normalize(flatten(forward)), normalize(flatten(direction))));
    const Vector side = normalize(cross(Vector{0.0f, 1.0f, 0.0f, 0.0f}, direction));
    const Vector approach = record.vector(96);
    const float approach_side = dot(side, approach);
    // 82E0915C/16C scalar skip branches, followed by ordered angular test.
    if (rs::abs(approach_side) <= 0.1f || dot(forward, side) * approach_side >= 0.0f || !(facing < 0.8f)) {
        return false;
    }
    const Vector reach = flatten(sub(point, position));
    if (!(dot(reach, reach) > 0.0f)) return false;
    if (!(limits.angle_a > angle(reach, rs::map(approach, [](float v) { return -v; })))) return false;
    // 82E09228..2A0 chooses a real neighboring point along the full polyline.
    float step = length * 0.5f;
    if (step > rs::from_bits(0x3c23d70au)) step = rs::from_bits(0x3c23d70au);
    const float next_distance = distance + step > length ? distance - step : distance + step;
    const Vector tangent = sub(polyline::at_distance(record, next_distance), point);
    const Vector horizontal = flatten(tangent);
    if (!(dot(horizontal, horizontal) > rs::from_bits(0x37800000u))) return false;
    const float turns = angle(tangent, horizontal) * rs::from_bits(0x3e22f983u);
    const float fraction = turns - rs::floor(turns);
    float slope = rs::abs((fraction - (fraction > 0.5f ? 1.0f : 0.0f)) * rs::from_bits(0x40c90fdbu));
    if (slope > rs::from_bits(0x3fc90fdbu)) slope = rs::from_bits(0x40490fdbu) - slope;
    if (!(limits.angle_b > slope)) return false;
    // 82E093E8 rigid inverse followed by inclusive bounds on all XYZ axes.
    const Vector local = inverse_point(bounds.frame, point);
    for (std::size_t i = 0; i < 3; ++i) {
        if (!(local[i] >= -bounds.extents[i] && local[i] <= bounds.extents[i])) return false;
    }
    return true;
}

// ---- qualify/selection.rs ----
// Original S3 82D4D150/82E09D28; S2 82D97D68 lacks S3 eligibility tests.
namespace {
float sel_dot(Vector a, Vector b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vector sel_sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
float sel_inverse_length(float squared) {
    float r = physics::reciprocal_sqrt::estimate(squared);
    for (int i = 0; i < 2; ++i) r = rs::mul_add(r * 0.5f, rs::mul_add(-squared, r * r, 1.0f), r);
    return r;
}
Vector sel_horizontal(Vector v) {
    v[1] = 0.0f;
    const float squared = sel_dot(v, v);
    const float r = sel_inverse_length(squared);
    const float length = squared == 0.0f ? 0.0f : squared * r;
    // 82F826F8 initializes830BD350 from82181A88, a LENGTH threshold.
    if (length > rs::from_bits(0x358637bdu)) return rs::map(v, [&](float x) { return x * r; });
    return Vector{};
}
}  // namespace

std::optional<Record> best_spline(const std::vector<Record>& records, Vector position) {
    float distance = rs::from_bits(0x47c34ff3u);  // 822F8FF0.
    std::optional<Record> best;
    for (const auto& record : records) {
        const auto endpoints = record.endpoints();
        const Vector delta = sel_sub(closest_point(position, endpoints), position);
        const Vector direction = sel_horizontal(sel_sub(endpoints[0], endpoints[1]));
        const Vector approach = sel_horizontal(delta);
        const bool eligible = delta[1] > rs::from_bits(0xbf19999au) && delta[1] < rs::from_bits(0x3f19999au) &&
                              rs::abs(sel_dot(direction, approach)) < rs::from_bits(0x3f4ccccdu);
        const float candidate_distance = sel_dot(delta, delta);
        // 82D4D39C is bge: unordered does not take the skip branch.
        if (!(candidate_distance >= distance)) {
            distance = candidate_distance;
            best = eligible ? std::optional<Record>(record) : std::nullopt;
        }
    }
    return best;
}

// ---- query.rs ----
rs::Result<std::vector<Record>, Error> query(const Registry& registry, const Query& q) {
    using namespace grab_scene::math;
    if (q.mode != 4) return rs::make_err(Error("This offboard provider implements native bounded mode4 only"));
    if (q.capacity > 32) return rs::make_err(Error("Native grab query buffer capacity is32"));
    bool nonfinite = false;
    for (float v : q.position) nonfinite = nonfinite || !rs::is_finite(v);
    for (float v : q.sort_position) nonfinite = nonfinite || !rs::is_finite(v);
    for (float v : q.bounds.extents) nonfinite = nonfinite || !rs::is_finite(v);
    for (const auto& row : q.bounds.frame)
        for (float v : row) nonfinite = nonfinite || !rs::is_finite(v);
    if (nonfinite) return rs::make_err(Error("Nonfinite grab query"));
    const float radius = length(q.bounds.extents);
    const Vector center = q.bounds.frame[3];
    std::vector<Record> out;
    out.reserve(32);
    for (const auto& object : registry.objects) {
        if (object.disabled || !object.assembly_ready || !object.assembly) continue;
        if (const auto* dmo = std::get_if<ProviderDmo>(&object.provider.value)) {
            if (!dmo->record_enabled ||
                std::uint32_t(dmo->selection_variant) != ((q.context.selection_flags_2948 >> 1) & 1u) ||
                !(q.context.matching_id_2952 == -1 || dmo->matching_group == -1 ||
                  dmo->matching_group == q.context.matching_id_2952)) {
                continue;
            }
        } else {
            const Vector d = sub(object.frame[3], center);
            const float limit = radius + 15.0f;
            if (dot(d, d) >= limit * limit) continue;
        }
        for (const auto& spline : object.splines) {
            if (spline.descriptor.kind != 2 || spline.geometry->points.size() < 2 ||
                spline.geometry->approach_vectors.empty()) {
                continue;
            }
            bool intersects = false;
            const auto& pts = spline.geometry->points;
            for (std::size_t k = 0; k + 1 < pts.size() && !intersects; ++k) {
                const Vector delta = sub(pts[k + 1], pts[k]);
                intersects = dot(delta, delta) > rs::from_bits(0x3727c5acu) &&
                             sphere_segment(center, radius, point(object.frame, pts[k]), point(object.frame, pts[k + 1]));
            }
            if (!intersects || out.size() == 32) continue;
            auto rec = object.record(spline);
            if (rec.is_err()) return rs::make_err(rec.unwrap_err());
            if (qualify(rec.unwrap(), q.position, q.bounds, q.limits)) out.push_back(rec.unwrap());
        }
    }
    // 82761C58 stores squared closest-segment distance at184. Since count<=32,
    // 82762178 always dispatches stable insertion sort827629D8 (strict shifts).
    for (auto& r : out) {
        const Vector delta = sub(q.sort_position, closest_point(q.sort_position, r.endpoints()));
        r._0[46] = rs::to_bits(dot(delta, delta));
    }
    for (std::size_t i = 1; i < out.size(); ++i) {
        std::size_t j = i;
        while (j > 0 && out[j].scalar(184) < out[j - 1].scalar(184)) {
            std::swap(out[j], out[j - 1]);
            j -= 1;
        }
    }
    if (out.size() > q.capacity) out.resize(q.capacity);
    return out;
}

}  // namespace s3::player::offboard::grab_scene
