// Port of skate-core/src/player/offboard/contact_toolkit.rs and its private submodules
// (analyzer_math, candidate, classification, collection, generation, infill, prefix,
// probes, profile, publication, samples, sweep).
#include "s3/core/player/offboard/contact_toolkit.hpp"

#include <algorithm>

#include "s3/core/player/offboard/contact_toolkit/analyzer_math.hpp"
#include "s3/core/player/offboard/contact_toolkit/stages.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::contact_toolkit {

// ---- contact_toolkit.rs: Owner::refresh ----
std::optional<Collected> Owner::refresh() {
    // 82D81610 resets the publication even when there is no pending batch.
    prefix_ = ContactPrefix::reset();
    readiness_ = 30;
    if (!pending_) return std::nullopt;
    auto [batch, results] = std::move(*pending_);
    pending_.reset();
    ContactPrefix prefix = ContactPrefix::reset();
    prefix.consume_support(batch.input, results.trajectories, candidate_.flags);
    Samples samples = collection::collect(batch, layout_, results);
    infill::insert_obstacles(batch.input, samples);
    infill::correct_normals(batch.input, samples);
    const auto [obstruction, active_count] = profile::simplify(batch.input, samples.ground);
    prefix.distance_172 = obstruction;
    const profile::Profile prof = profile::build(batch.input, samples.ground.data(), active_count);
    std::vector<candidate::Candidate> candidates =
        generation::generate(batch.input, prof, prefix, obstruction, candidate_.flags);
    publication::publish(batch.input, prof, samples, candidates, obstruction, candidate_, history_, prefix);
    prefix_ = prefix;
    return Collected{std::move(batch), std::move(results), prefix, std::move(samples)};
}

// ---- analyzer_math.rs ----
namespace analyzer_math {
std::optional<Vector> plane_segment(Vector p, Vector n, Vector a, Vector b) {
    const float da = dot(n, sub(a, p));
    const float db = dot(n, sub(b, p));
    if (da * db >= 0.0f) return std::nullopt;
    const float aa = rs::abs(da);
    const float bb = rs::abs(db);
    const float sum = aa + bb;
    return madd(a, bb / sum, scale(b, aa / sum));
}
Vector clamp_normal(Vector a, Vector b, Vector n) {
    a = normalize(a, ZERO);
    b = normalize(b, ZERO);
    const Vector axis = normalize(cross(a, b), ZERO);
    const Vector projected = normalize(sub(n, scale(axis, dot(axis, n))), ZERO);
    if (((dot(axis, axis) * dot(projected, projected)) * dot(a, a)) * dot(b, b) < 0.1f) {
        return n;
    }
    if (dot(cross(a, projected), axis) > 0.0f && dot(cross(projected, b), axis) > 0.0f) {
        return projected;
    } else if (dot(projected, a) > dot(projected, b)) {
        return a;
    }
    return b;
}
float intersection(std::array<float, 2> a, std::array<float, 2> b, std::array<float, 2> c, std::array<float, 2> d) {
    const std::array<float, 2> u{b[0] - a[0], b[1] - a[1]};
    const std::array<float, 2> v{d[0] - c[0], d[1] - c[1]};
    const float denominator = v[1] * u[0] - v[0] * u[1];
    if (rs::abs(denominator) < 0.00001f) return 1e10f;
    return (1.0f / denominator) * (v[0] * (a[1] - c[1]) - v[1] * (a[0] - c[0]));
}
float tangent(float x) {
    if (x == 0.0f) return 0.0f;
    auto c = [](std::uint32_t bits) { return rs::from_bits(bits); };
    const float turns = rs::round_ties_even(x * c(0x3f22f983u));
    const float r = rs::mul_add(-turns, c(0x2e85a309u), rs::mul_add(-turns, c(0x3fc90fdbu), x));
    const float q = r * r;
    // IDA's VMX operand display is A,B,C: vmaddfp computes A*C+B.
    // 8252DA20..38: denominator degree four, numerator correction degree two.
    float den = rs::mul_add(
        q,
        rs::mul_add(q, rs::mul_add(q, rs::mul_add(q, c(0x3505bba8u), c(0xb9a37b25u)), c(0x3cd23cf5u)), c(0xbeeef582u)),
        1.0f);
    const float poly = rs::mul_add(q, rs::mul_add(q, c(0xb795d5b9u), c(0x3b607415u)), c(0xbe0895afu));
    float num = rs::mul_add(r, q * poly, r);
    if (rs::abs(r) <= c(0x39800000u)) {
        num = r;
        den = 1.0f;
    }
    if ((rs::as_i32(rs::abs(turns)) & 1) == 0) {
        return num * reciprocal(den);
    }
    return den * reciprocal(-num);
}
}  // namespace analyzer_math

using analyzer_math::madd;

// ---- candidate.rs ----
namespace candidate {
void Builder::add(Vector position, Vector normal, Vector direction, std::int32_t segment, std::uint32_t flags,
                  std::uint32_t kind) {
    const float order = dot(input.forward, sub(position, input.position));
    Candidate c{position, normal, direction, flags, order, order, order, segment, kind};
    if (segment < 0) {
        c.low = 0.0f;
    } else {
        const auto& s = profile->segments[std::size_t(segment)];
        if (s.kind != 0) {
            const Vector point = s.kind == 2 ? s.start : s.end;
            const float distance = dot(input.forward, sub(point, input.position));
            if (distance >= order) {
                c.high = distance;
            } else {
                c.low = distance;
            }
        }
    }
    candidates.push_back(c);
}
void Builder::point(Vector position, Vector normal, std::int32_t segment, std::uint32_t flags, std::uint32_t kind) {
    const Vector delta = sub(position, input.position);
    add(position, normal, length(delta) >= 0.02f ? delta : input.forward, segment, flags, kind);
}
std::array<float, 2> Builder::local(Vector p) const {
    const Vector d = sub(p, input.position);
    return {dot(input.forward, d), dot(input.up, d)};
}
void Builder::approach_up(std::size_t index, std::uint32_t flags, float slope) {
    const auto p = local(profile->segments[index].end);
    const std::array<float, 2> goal{-1.0f, rs::mul_add(-1.0f - p[0], slope, p[1])};
    for (std::size_t i = index; i-- > 0;) {
        const auto s = profile->segments[i];
        const float t = analyzer_math::intersection(local(s.start), local(s.end), p, goal);
        if (t < 0.0f) {
            if (i == 0) {
                point(input.position, input.up, std::int32_t(index), flags, 1);
                return;
            }
        } else if (t <= 1.0f) {
            Vector position, normal;
            if (s.kind == 0) {
                position = madd(s.direction, s.length * t, s.start);
                normal = s.normal;
            } else {
                position = s.start;
                normal = next_normal(i);
            }
            if (local(position)[0] <= 0.75f) {
                point(position, normal, std::int32_t(index), flags, 1);
            }
            return;
        }
    }
}
void Builder::approach_down(std::int32_t index, std::uint32_t flags, float slope, std::uint32_t kind) {
    const Vector pt = index >= 0 ? profile->segments[std::size_t(index)].start : input.position;
    const auto p = local(pt);
    const std::array<float, 2> goal{2.8f, rs::mul_add(2.8f - p[0], slope, p[1])};
    for (std::size_t i = std::size_t(index + 1); i < profile->last_ground; ++i) {
        const auto s = profile->segments[i];
        const float t = analyzer_math::intersection(local(s.start), local(s.end), p, goal);
        if (t >= 0.0f && t <= 1.0f) {
            if (s.kind == 0) {
                point(madd(s.direction, s.length * t, s.start), s.normal, index, flags, kind);
            } else if (local(s.end)[0] <= obstruction - 0.35f) {
                point(s.end, next_normal(i), index, flags, kind);
            }
            return;
        }
    }
}
void Builder::advance(float distance) {
    for (std::size_t i = 0; i < profile->last_ground; ++i) {
        const auto s = profile->segments[i];
        if (s.kind == 2) {
            distance += s.length / down_slope(input, s.length);
            continue;
        }
        distance -= s.length;
        if (distance > 0.0f) continue;
        if (s.kind == 1) {
            point(s.end, next_normal(i), std::int32_t(i), 0, 9);
        } else {
            point(madd(s.direction, distance, s.end), s.normal, std::int32_t(i), 0, 9);
        }
        return;
    }
}
Vector Builder::next_normal(std::size_t i) const {
    return i == profile->last_ground - 1 ? input.up : profile->segments[i + 1].normal;
}
namespace {
float slope(const Input& input, float low, float high) {
    const float speed = rs::min(rs::max(length(input.velocity), 2.0f), 6.0f);
    return analyzer_math::tangent((((speed - 2.0f) * (low - high)) * 0.25f + high) * 0.017453292f);
}
}  // namespace
float up_slope(const Input& input, float height) {
    return height > 0.4f ? slope(input, 35.0f, 50.0f) : slope(input, 15.0f, 30.0f);
}
float down_slope(const Input& input, float height) {
    return height > 0.4f ? slope(input, 30.0f, 50.0f) : slope(input, 15.0f, 30.0f);
}
float between_slope(const Input& input, Vector a, Vector b) {
    const Vector d = sub(b, a);
    const float x = dot(input.forward, d);
    if (x < 0.001f) return 1000.0f;
    return analyzer_math::reciprocal(x) * dot(input.up, d);
}
}  // namespace candidate

// ---- classification.rs ----
namespace classification {
void History::classify(ContactPrefix& prefix, Vector direction) {
    const float horizontal = length(Vector{direction[0], 0.0f, direction[2], 0.0f});
    // 82D84994 uses scalar division, not the vector reciprocal helper.
    const float inverse = 1.0f / (0.001f - horizontal >= 0.0f ? 0.001f : horizontal);
    const float slope = inverse * direction[1];
    prefix.scalar_160 = slope;
    // Stock globals 822F8FCC/822F8FD0; arguments are radians.
    const float low = analyzer_math::tangent(rs::from_bits(0x3dd67750u));
    const float high = analyzer_math::tangent(rs::from_bits(0x3f0efa35u));
    const float magnitude = rs::abs(slope);
    std::uint32_t proposed;
    if (magnitude <= high) {
        proposed = magnitude <= low ? 0 : (slope > 0.0f ? 1 : 3);
    } else {
        proposed = slope > 0.0f ? 2 : 4;
    }
    if (proposed != 0 && (prefix.flags_176 & 0x40u) == 0) proposed += 4;
    prefix.kind_164 = proposed;
    if (proposed == 0) {
        *this = History{};
    } else if (samples[0] == proposed && samples[1] == proposed && samples[2] == proposed) {
        accepted = proposed;
    } else {
        cursor = (cursor + 1) % 3;
        samples[cursor] = proposed;
        prefix.kind_164 = accepted;
    }
}
}  // namespace classification

// ---- collection.rs ----
namespace collection {
namespace {
/// 8296EBB0 followed by 82E09C80. One inverse-sqrt refinement per input.
float folded_angle(Vector a, Vector b) {
    const float aa = dot(a, a);
    const float bb = dot(b, b);
    float angle = 0.0f;
    if (aa > rs::from_bits(0x38d1b717u) && bb > rs::from_bits(0x38d1b717u)) {
        auto unit = [](Vector v, float square) {
            const float r = physics::reciprocal_sqrt::estimate(square);
            return scale(v, rs::mul_add(r * 0.5f, rs::mul_add(-square, r * r, 1.0f), r));
        };
        angle = trigonometry::acos(rs::clamp(dot(unit(a, aa), unit(b, bb)), -1.0f, 1.0f));
    }
    const float turns = angle * rs::from_bits(0x3e22f983u);
    const float fraction = turns - rs::floor(turns);
    const float signed_ = (fraction - (fraction > 0.5f ? 1.0f : 0.0f)) * rs::from_bits(0x40c90fdbu);
    const float sign = signed_ <= 0.0f ? -1.0f : 1.0f;
    const float magnitude = signed_ * sign;
    return sign * (magnitude > 1.5707964f ? magnitude - 3.1415927f : magnitude);
}
}  // namespace

Samples collect(const Batch& batch, const ProbeLayout& layout, const QueryResults& results) {
    const Input input = batch.input;
    Samples samples;
    const auto main = results.trajectories[0];
    if (main.valid()) {
        samples.insert(input, main.contact_position, main.landing_normal, 1, -1.0f, 0);
    }
    std::vector<std::optional<Vector>> edge_points(batch.secondary.size());
    for (const auto& e : results.edges) {
        const Vector start = e[0], end = e[1];
        const Vector delta = sub(end, start);
        if (rs::abs(folded_angle(delta, input.forward)) > 30.0f * 0.017453292f) {
            if (auto point = analyzer_math::plane_segment(input.position, input.right, start, end)) {
                samples.insert(input, *point, input.up, 2, -1.0f, 0);
            }
            continue;
        }
        const float a = dot(sub(start, input.position), input.forward);
        const float b = dot(sub(end, input.position), input.forward);
        const Vector at_origin = madd(delta, a / (a - b), start);
        if (rs::abs(dot(input.right, sub(at_origin, input.position))) > 0.5f) continue;
        const std::size_t n = std::min({layout.secondary.size(), batch.secondary.size(), edge_points.size()});
        for (std::size_t k = 0; k < n; ++k) {
            const auto& local = layout.secondary[k];
            const auto& world = batch.secondary[k];
            auto& point = edge_points[k];
            const float z = local.line.start[2];
            if (z < rs::min(a, b) || z > rs::max(a, b)) continue;
            const Vector on_edge = madd(delta, (z - a) / (b - a), start);
            const Vector projected =
                madd(input.up, dot(input.up, sub(on_edge, world.line.start)), world.line.start);
            if (rs::abs(dot(input.right, sub(projected, on_edge))) >= 0.05f) continue;
            if (!point || projected[1] > (*point)[1]) point = projected;
        }
    }
    for (std::size_t k = 0; k < batch.secondary.size() && k < edge_points.size(); ++k) {
        const auto& descriptor = batch.secondary[k];
        const auto& edge = edge_points[k];
        const auto& hit = results.lines[descriptor.forward_index];
        if (hit && edge && (*edge)[1] > hit->position[1]) {
            samples.insert(input, *edge, input.up, 1, -1.0f, 0);
        } else if (hit) {
            samples.insert(input, hit->position, hit->normal, 1, -1.0f, 0);
        } else if (edge) {
            samples.insert(input, *edge, input.up, 1, -1.0f, 0);
        }
    }
    for (const auto& descriptor : batch.primary) {
        std::array<std::optional<std::size_t>, 2> indices{descriptor.forward_index, descriptor.reverse_index};
        for (const auto& index : indices) {
            if (!index) continue;
            if (const auto& hit = results.lines[*index]) {
                samples.insert(input, hit->position, hit->normal, 0, -1.0f, 0);
            }
        }
    }
    profile::sort(samples.ground);
    profile::sort(samples.other);
    return samples;
}
}  // namespace collection

// ---- generation.rs ----
namespace generation {
std::vector<candidate::Candidate> generate(const Input& input, const profile::Profile& profile,
                                           const ContactPrefix& prefix, float obstruction,
                                           std::uint32_t retained_flags) {
    using candidate::between_slope;
    using candidate::down_slope;
    using candidate::up_slope;
    candidate::Builder builder{input, &profile, obstruction, {}};
    const bool supported = (prefix.flags_176 & 1u) != 0;
    if (supported) {
        const Vector planar = sub(input.velocity, scale(input.up, dot(input.up, input.velocity)));
        if (length(planar) < 0.01f) {
            builder.add(prefix.position, prefix.normal, cross(input.right, prefix.normal), -1, 0, 0);
            return builder.candidates;
        }
    }
    if (profile.last_ground == 0) return builder.candidates;
    std::optional<std::size_t> highest;
    bool have_rise = false;
    bool saw_drop = false;
    auto forward = [&](Vector point) { return dot(sub(point, input.position), input.forward); };
    for (std::size_t i = 0; i < profile.last_ground; ++i) {
        const auto segment = profile.segments[i];
        Vector normal;
        if (i + 1 < profile.last_ground) {
            const auto next = profile.segments[i + 1];
            normal = (segment.kind != 0 && next.kind != 0) ? input.up : next.normal;
        } else {
            normal = segment.normal;
        }
        if (!saw_drop) {
            const Vector prior = highest ? profile.segments[*highest].end : input.position;
            if (dot(sub(segment.end, prior), input.up) > 0.0f) highest = i;
        }
        if (segment.kind == 1 && segment.length > 0.05f) {
            const bool tall = segment.length > 0.4f;
            bool combined = false;
            bool eligible = false;
            float slope = up_slope(input, segment.length);
            std::size_t j = i + 1;
            while (j < profile.last_ground) {
                const auto next = profile.segments[j];
                if (!eligible && next.kind == 2) {
                    eligible = forward(next.start) < obstruction - 0.35f;
                }
                if (next.kind != 0 && (next.kind != 1 || next.length >= 0.05f)) break;
                if (!eligible) {
                    eligible = dot(next.direction, input.forward) > 0.70700002f &&
                               forward(next.start) < obstruction - 0.35f;
                }
                j += 1;
            }
            if (j < profile.last_ground && profile.segments[j].kind == 1) {
                const Vector next_end = profile.segments[j].end;
                const float joined = between_slope(input, segment.end, next_end);
                if (joined > up_slope(input, segment.length) * 0.5f) {
                    slope = joined;
                    combined = true;
                    if (!have_rise) {
                        combined = between_slope(input, input.position, next_end) >=
                                   up_slope(input, segment.length) * 0.5f;
                    }
                }
            }
            if (!have_rise) have_rise = eligible;
            if (eligible) {
                builder.approach_up(i, tall ? 0x80u : 0u, slope);
                const std::uint32_t flags = 0x40u | ((combined || !tall) ? 0u : 0x100u);
                builder.point(segment.end, normal, std::int32_t(i), flags, combined ? 2 : 3);
            }
        } else if (segment.kind == 2 && segment.length > 0.05f) {
            const bool tall = segment.length > 0.4f;
            bool combined = false;
            float slope = -down_slope(input, segment.length);
            std::size_t j = i + 1;
            while (j < profile.last_ground) {
                const auto next = profile.segments[j];
                if (next.kind != 0 && !(next.kind == 2 && next.length < 0.05f)) break;
                j += 1;
            }
            if (j < profile.last_ground && profile.segments[j].kind == 2) {
                const float joined = between_slope(input, segment.start, profile.segments[j].start);
                if (joined < down_slope(input, segment.length) * -0.5f) {
                    combined = true;
                    slope = joined;
                }
            }
            bool suppress = false;
            if (!saw_drop && !have_rise) {
                if (highest) {
                    suppress = dot(sub(profile.segments[*highest].end, input.position), input.up) > 0.02f;
                    if (i == 1 && profile.segments[0].kind == 0) suppress = false;
                }
            }
            saw_drop = true;
            if (!suppress) {
                if (combined) {
                    builder.point(segment.start, normal, std::int32_t(i), 0x40, 5);
                } else if (forward(segment.start) <= 0.75f) {
                    builder.approach_down(std::int32_t(i), tall ? 0x100u : 0u, slope, 4);
                    builder.point(segment.start, normal, std::int32_t(i), 0x40u | (tall ? 0x80u : 0u), 6);
                }
            }
        }
    }
    const float gap = input.position[1] - prefix.position[1];
    if (supported && gap > 0.01f) {
        const std::uint32_t flags = gap >= 0.4f ? ((retained_flags & 0x100u) | 0x40u) : (retained_flags & 0x140u);
        if (builder.candidates.empty() || !saw_drop) {
            const std::uint32_t kind = builder.candidates.empty() ? 7 : 8;
            // 82D83F94 loads stock 8208824C=0x3F4CCCCD, not `gap`.
            builder.approach_down(-1, flags, -down_slope(input, 0.8f), kind);
        }
    } else if (builder.candidates.empty()) {
        builder.advance(length(input.velocity) * 0.016666668f);
    }
    return builder.candidates;
}
}  // namespace generation

// ---- infill.rs ----
namespace infill {
namespace {
void infill(const Input& input, Samples& s, ContactSample a, ContactSample b, std::size_t index) {
    using analyzer_math::clamp_normal;
    using analyzer_math::plane_segment;
    const ContactSample p = s.other[index];
    if (dot(a.normal, b.normal) > 0.99f && dot(a.normal, p.normal) > 0.99f &&
        rs::abs(dot(sub(b.position, a.position), b.normal)) < 0.01f) {
        Vector delta = sub(p.position, a.position);
        if (length(delta) < 0.01f) delta = sub(p.position, b.position);
        if (rs::abs(dot(delta, b.normal)) < 0.01f) return;
    }
    const Vector n = clamp_normal(b.height > a.height ? scale(input.forward, -1.0f) : input.forward, input.up, p.normal);
    const Vector delta = sub(b.position, a.position);
    const Vector first_end = madd(sub(delta, scale(a.normal, dot(a.normal, delta))), 1.1f, a.position);
    const Vector last_start = sub(b.position, scale(sub(delta, scale(b.normal, dot(b.normal, delta))), 1.1f));
    const std::array<std::optional<Vector>, 2> cands{plane_segment(p.position, n, a.position, first_end),
                                                     plane_segment(p.position, n, last_start, b.position)};
    for (const auto& c : cands) {
        if (!c) continue;
        const Vector candidate = *c;
        const Vector d = sub(candidate, input.position);
        const float x = dot(input.forward, d);
        const float y = dot(input.up, d);
        if (x >= a.forward_distance && x <= b.forward_distance && y >= rs::min(a.height, b.height) &&
            y <= rs::max(a.height, b.height)) {
            const float t = length(sub(candidate, a.position)) / length(delta);
            const float order = t * (b.sort_distance - a.sort_distance) + a.sort_distance;
            s.insert(input, candidate, n, 1, order, 2);
            s.other[index].flags |= 4;
        }
    }
}
}  // namespace

void insert_obstacles(const Input& input, Samples& s) {
    const std::vector<ContactSample> initial = s.ground;
    for (std::size_t w = 0; w + 1 < initial.size(); ++w) {
        const ContactSample a = initial[w], b = initial[w + 1];
        float low = rs::min(a.height, b.height);
        float high = rs::max(a.height, b.height);
        std::size_t i = 0;
        while (i < s.other.size()) {
            const ContactSample p = s.other[i];
            if (a.forward_distance > p.forward_distance || p.forward_distance > b.forward_distance) {
                i += 1;
                continue;
            }
            if (low > p.height) {
                s.other[i].flags |= 2;
                i += 1;
                continue;
            }
            if (high > p.height || p.sort_distance < a.sort_distance || p.sort_distance > b.sort_distance) {
                i += 1;
                continue;
            }
            std::size_t minimum = i;
            std::size_t maximum = i;
            s.other[i].flags |= 2;
            i += 1;
            while (i < s.other.size() && rs::abs(p.forward_distance - s.other[i].forward_distance) <= 0.001f) {
                const ContactSample q = s.other[i];
                s.other[i].flags |= 2;
                if (q.height >= high && s.other[minimum].height > q.height) minimum = i;
                if (q.height > s.other[maximum].height) maximum = i;
                i += 1;
            }
            for (std::size_t index : {minimum, maximum}) {
                s.other[index].flags = (s.other[index].flags & ~2u) | 1u;
            }
            const ContactSample pm = s.other[minimum];
            s.insert(input, pm.position, pm.normal, 1, pm.sort_distance, 1);
            if (pm.height > high) {
                low = high;
                high = pm.height;
            } else if (pm.height > low) {
                low = pm.height;
            }
            if (minimum != maximum) {
                const ContactSample q = s.other[maximum];
                s.insert(input, q.position, q.normal, 1, q.sort_distance, 1);
            }
            // 82D82498 advances once more at the outer for-loop increment,
            // after the equal-forward-distance group has already advanced i.
            i += 1;
        }
    }
    profile::sort(s.ground);
}

void correct_normals(const Input& input, Samples& s) {
    Vector previous = input.position;
    for (auto& p : s.ground) {
        const float height = dot(sub(p.position, previous), input.up);
        p.normal = rs::abs(height) < 0.001f
                       ? input.up
                       : analyzer_math::clamp_normal(height <= 0.0f ? input.forward : scale(input.forward, -1.0f),
                                                     input.up, p.normal);
        previous = p.position;
    }
    const std::vector<ContactSample> initial = s.ground;
    for (std::size_t w = 0; w + 1 < initial.size(); ++w) {
        for (std::size_t i = 0; i < s.other.size(); ++i) {
            const ContactSample p = s.other[i];
            if ((p.flags & 3u) == 0 && initial[w].forward_distance <= p.forward_distance &&
                p.forward_distance <= initial[w + 1].forward_distance) {
                infill(input, s, initial[w], initial[w + 1], i);
            }
        }
    }
    profile::sort(s.ground);
}
}  // namespace infill

// ---- prefix.rs ----
void ContactPrefix::consume_support(const Input& input, const std::array<air::trajectory::QueryResult, 3>& hits,
                                    std::uint32_t candidate_flags) {
    const auto main = hits[0];
    if (main.valid()) {
        position = main.contact_position;
        normal = main.landing_normal;
        support_frame = main.contact_transform;
        flags_176 |= 1;
        if (dot(input.up, sub(main.contact_position, input.position)) < -0.1f) {
            flags_176 |= 8;
            if ((candidate_flags & 0x40u) != 0) flags_176 |= 0x40;
        } else {
            flags_176 &= ~8u;
        }
        support_180 = main.geometry;
    }
    const std::pair<air::trajectory::QueryResult, std::uint32_t> sides[2] = {{hits[1], 0x10u}, {hits[2], 0x20u}};
    for (const auto& [hit, flag] : sides) {
        // 82D85660 strict bounds, no contact-time proximity surrogate.
        const float height = dot(sub(hit.contact_position, input.position), input.up);
        if (hit.valid() && height < 0.2f && height > -0.2f) {
            flags_176 |= flag;
            if (main.valid() && hit.landing_normal[1] > normal[1]) normal = hit.landing_normal;
        }
    }
}

// ---- probes.rs ----
namespace {
LineProbe probe(float x0, float y0, float z0, float x1, float y1, float z1, float radius) {
    return LineProbe{{x0, y0, z0, 0.0f}, {x1, y1, z1, 0.0f}, radius};
}
LineProbe transform(LineProbe line, const std::array<Vector, 4>& frame) {
    auto point = [&](Vector v) {
        return rs::from_fn<4>([&](std::size_t i) {
            return rs::mul_add(frame[2][i], v[2],
                               rs::mul_add(frame[1][i], v[1], rs::mul_add(frame[0][i], v[0], frame[3][i])));
        });
    };
    return LineProbe{point(line.start), point(line.end), line.radius};
}
}  // namespace

ProbeLayout ProbeLayout::stock() {
    // Live TU3 data8303744C = 3f4ccccd (0.8); not a tuned foot height.
    ProbeLayout out;
    out.trajectories = {
        probe(0.0f, 1.2f, 0.0f, 0.0f, -0.8f, 0.0f, 0.01f),
        probe(-0.12f, 0.8f, 0.0f, -0.17f, -0.2f, 0.0f, 0.03f),
        probe(0.12f, 0.8f, 0.0f, 0.17f, -0.2f, 0.0f, 0.03f),
    };
    std::size_t index = 0;
    float z = 0.037500001f;
    while (true) {
        out.secondary.push_back(Descriptor{probe(0.0f, 0.8f, z, 0.0f, -0.8f, z, 0.0f), index, std::nullopt});
        index += 1;
        z += 0.075000003f;
        if (!(z < 1.7624999f)) break;
    }
    for (int i = 0; i < 12; ++i) {
        // Preserve the source's two products and subtraction.
        const float fraction = float(i) * 0.090909094f;
        const float height = fraction * 0.8f - (1.0f - fraction) * 0.8f;
        const std::optional<std::size_t> reverse_index =
            height < 0.0f ? std::optional<std::size_t>(index + 1) : std::nullopt;
        out.primary.push_back(Descriptor{probe(0.0f, height, 0.0f, 0.0f, height, 1.8f, 0.0f), index, reverse_index});
        index += reverse_index ? 2 : 1;
    }
    for (int i = 0; i < 3; ++i) {
        const float height = (float(i) * 2.0f + 1.0f) * 0.099999994f + 0.8f;
        out.primary.push_back(Descriptor{probe(0.0f, height, 0.099999994f, 0.0f, height, 1.6999999f, 0.099999994f),
                                         index, std::nullopt});
        index += 1;
    }
    return out;
}

Batch ProbeLayout::prepare(const Input& input, std::int32_t matching_group) const {
    const std::array<Vector, 4> surface{input.right, input.up, input.forward, input.position};
    // 82D81298..12E4 computes animation_right x animation_up.
    const std::array<Vector, 4> animated{input.animation_right, input.animation_up,
                                         cross(input.animation_right, input.animation_up), input.position};
    Batch batch;
    batch.input = input;
    batch.matching_group = matching_group;
    batch.mesh_reject_mask = 0x6000;
    for (std::size_t i = 0; i < 3; ++i) {
        const LineProbe line = transform(trajectories[i], i == 0 ? surface : animated);
        air::trajectory::QueryRequest r;
        r.trajectory.position = line.start;
        r.trajectory.velocity = sub(line.end, line.start);
        r.trajectory.acceleration = Vector{};
        r.trajectory.duration = 1.0f;
        r.radius = line.radius;
        r.start_error = 0.0f;
        r.end_error = 0.0f;
        batch.trajectories[i] = r;
    }
    auto prep = [&](const std::vector<Descriptor>& descriptors) {
        std::vector<Descriptor> out;
        for (const auto& d : descriptors) {
            const LineProbe line = transform(d.line, surface);
            batch.lines.push_back(line);
            if (d.reverse_index) batch.lines.push_back(LineProbe{line.end, line.start, line.radius});
            Descriptor nd = d;
            nd.line = line;
            out.push_back(nd);
        }
        return out;
    };
    batch.secondary = prep(secondary);
    batch.primary = prep(primary);
    return batch;
}

// ---- profile.rs ----
namespace profile {
std::pair<float, std::size_t> simplify(const Input& input, std::vector<ContactSample>& points) {
    float obstruction = 1e10f;
    if (points.size() < 2) return {obstruction, points.size()};
    std::size_t count = points.size();
    const float removed = points.back().sort_distance + 1.0f;
    for (std::size_t i = 1; i < points.size() - 1; ++i) {
        if ((points[i].flags & 8u) != 0) {
            count -= points.size() - 1 - i;
            obstruction = dot(sub(points[i].position, input.position), input.forward);
            break;
        }
        const Vector incoming{points[i].forward_distance - points[i - 1].forward_distance,
                              points[i].height - points[i - 1].height, 0.0f, 0.0f};
        const Vector outgoing{points[i + 1].forward_distance - points[i].forward_distance,
                              points[i + 1].height - points[i].height, 0.0f, 0.0f};
        const float a = length(incoming);
        const float b = length(outgoing);
        const bool discard =
            a < 0.02f || b < 0.02f ||
            (!(dot(incoming, outgoing) < 0.0f && dot(incoming, input.up) >= 0.0f) &&
             length(cross(scale(incoming, analyzer_math::reciprocal(a)), scale(outgoing, analyzer_math::reciprocal(b)))) <
                 0.05f);
        if (discard) {
            points[i].sort_distance = removed;
            count -= 1;
        }
    }
    sort(points);
    // Native count14928 shrinks, but backing records survive for the later
    // publication scan bounded by retained original count14932.
    return {obstruction, count};
}
void sort(std::vector<ContactSample>& points) {
    std::stable_sort(points.begin(), points.end(),
                     [](const ContactSample& a, const ContactSample& b) { return a.sort_distance < b.sort_distance; });
}
Profile build(const Input& input, const ContactSample* points, std::size_t count) {
    using analyzer_math::normalize;
    using analyzer_math::reciprocal;
    Profile out;
    for (std::size_t w = 0; w + 1 < count; ++w) {
        const Vector start = points[w].position;
        const Vector end = points[w + 1].position;
        const Vector delta = sub(end, start);
        const float distance = length(delta);
        if (distance < 0.0001f) continue;
        const Vector direction = scale(delta, reciprocal(distance));
        std::uint32_t kind;
        if (rs::abs(dot(input.forward, direction)) > 0.5f) {
            kind = 0;
        } else if (dot(input.up, direction) >= 0.0f) {
            kind = 1;
        } else {
            kind = 2;
        }
        if (kind != 0 && !out.segments.empty() && out.segments.back().kind == kind) {
            Segment& last = out.segments.back();
            last.end = end;
            const Vector d = sub(end, last.start);
            last.length = length(d);
            last.direction = scale(d, reciprocal(last.length));
            last.normal = normalize(cross(d, input.right), input.up);
        } else {
            out.segments.push_back(Segment{kind, start, end, direction, normalize(cross(delta, input.right), input.up),
                                           distance});
            if (kind == 0) out.last_ground = out.segments.size();
        }
    }
    return out;
}
}  // namespace profile

// ---- publication.rs ----
namespace publication {
void publish(const Input& input, const profile::Profile& profile, const Samples& samples,
             std::vector<candidate::Candidate>& candidates, float obstruction, candidate::Candidate& retained,
             classification::History& history, ContactPrefix& prefix) {
    prefix.edge_position = input.position;
    prefix.edge_normal = input.up;
    prefix.distance_168 = 0.0f;
    if (profile.last_ground != 0) {
        const Vector end = profile.segments[profile.last_ground - 1].end;
        const float reach = dot(sub(end, input.position), input.forward);
        prefix.distance_168 = reach >= 1.65f ? 1e10f : reach;
        for (const auto& s : profile.segments) {
            if ((s.kind == 1 || s.kind == 2) && s.length > 0.05f) {
                prefix.distance_168 = dot(sub(s.start, input.position), input.forward);
                break;
            }
        }
        if ((prefix.flags_176 & 1u) != 0 && input.forward[1] > 0.2f && prefix.normal[1] < 0.7f) {
            const std::size_t ground_n = std::min(samples.original_ground_count, samples.ground.size());
            const std::pair<const ContactSample*, std::size_t> lists[2] = {
                {samples.ground.data(), ground_n}, {samples.other.data(), samples.other.size()}};
            for (const auto& [ptr, n] : lists) {
                for (std::size_t k = 0; k < n; ++k) {
                    const ContactSample& point = ptr[k];
                    if (point.normal[1] > 0.95f || (point.flags & 0x10u) != 0) {
                        if (dot(sub(point.position, input.position), input.up) > 0.0f) prefix.flags_176 |= 0x200;
                        break;
                    }
                }
            }
        }
        float distance = length(input.velocity) * 0.25f;
        for (const auto& segment : profile.segments) {
            if (segment.kind != 0 && segment.normal[1] <= 0.9f) continue;
            prefix.edge_normal = segment.normal;
            distance -= segment.length;
            if (distance <= 0.0f) {
                prefix.edge_position = madd(segment.direction, distance, segment.end);
                break;
            }
            prefix.edge_position = segment.end;
            prefix.flags_176 |= 4;
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const candidate::Candidate& a, const candidate::Candidate& b) { return a.order < b.order; });
    if (candidates.empty()) {
        if (obstruction >= 0.3f) {
            prefix.flags_176 &= ~1u;
        } else {
            prefix.flags_176 |= 0x400;
        }
        return;
    }
    const float step = length(input.velocity) * 0.016666668f;
    std::size_t selected = candidates.size() - 1;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const auto& c = candidates[index];
        const bool behind = index + 1 < candidates.size() && dot(input.forward, sub(c.position, input.position)) < step;
        if (c.kind == 1) {
            bool overridden = behind;
            if (!overridden) {
                for (std::size_t k = index + 1; k < candidates.size(); ++k) {
                    const auto& next = candidates[k];
                    if (!(next.low <= c.high)) break;
                    if (next.kind == 4 || next.kind == 7 || next.kind == 8) {
                        overridden = true;
                        break;
                    }
                }
            }
            if (overridden) {
                selected = index;
                for (std::size_t k = index + 1; k < candidates.size(); ++k) {
                    if (candidates[k].segment == c.segment) {
                        selected = k;
                        break;
                    }
                }
                break;
            }
        }
        if (!behind) {
            selected = index;
            break;
        }
    }
    retained = candidates[selected];
    prefix.target_position = retained.position;
    prefix.target_normal = retained.normal;
    prefix.flags_176 |= retained.flags | 2u;
    if ((prefix.flags_176 & 1u) == 0) {
        prefix.flags_176 |= 0x3b;
        prefix.position = input.position;
        prefix.normal = input.up;
    }
    history.classify(prefix, retained.direction);
}
}  // namespace publication

// ---- samples.rs ----
bool Samples::insert(const Input& input, Vector position, Vector normal, std::uint32_t category, float sort_override,
                     std::uint32_t provenance) {
    const Vector delta = sub(position, input.position);
    const float forward = dot(input.forward, delta);
    const float height = dot(input.up, delta);
    const bool high = height >= 0.79000002f && forward >= 0.0f;
    normal = sub(normal, scale(input.right, dot(normal, input.right)));
    const float magnitude = length(normal);
    if (magnitude >= 0.001f) {
        normal = scale(normal, analyzer_math::reciprocal(magnitude));
    } else if (height < 0.0f) {
        normal = input.forward;
    } else {
        normal = scale(input.forward, -1.0f);
    }
    const bool is_ground = category == 1 || high;
    if ((is_ground && ground.size() == 64) || (!is_ground && forward < 0.0f)) return false;
    std::uint32_t flags = provenance == 1 ? 1u : provenance == 2 ? 4u : 0u;
    if (high) flags |= 8;
    if (category == 2) flags |= 0x10;
    const ContactSample sample{position, normal, forward, height, flags, sort_override >= 0.0f ? sort_override : forward};
    if (is_ground) {
        ground.push_back(sample);
        original_ground_count = ground.size();
    } else {
        other.push_back(sample);
    }
    return true;
}

// ---- sweep.rs ----
Vector sweep_detail::landing_normal(const std::vector<std::array<Vector, 3>>& triangles, Vector velocity) {
    std::vector<Vector> normals;
    Vector best = ZERO;
    float best_y = -1.0f;
    const std::size_t n_tri = std::min<std::size_t>(triangles.size(), 64);
    for (std::size_t k = 0; k < n_tri; ++k) {
        const auto& t = triangles[k];
        Vector n = cross(sub(t[1], t[0]), sub(t[2], t[0]));
        n = scale(n, inverse_length(dot(n, n)));
        if (n[1] > 0.7f || dot(n, velocity) <= 0.0f) {
            if (n[1] > best_y) {
                best = n;
                best_y = n[1];
            }
            normals.push_back(n);
        }
    }
    if (normals.empty()) return UP;
    Vector total = ZERO;
    for (const auto& n : normals) {
        if (dot(best, n) > 0.5f) {
            for (int i = 0; i < 4; ++i) total[i] += n[i];
        }
    }
    const float square = dot(total, total);
    const float inverse = inverse_length(square);
    const float magnitude = square == 0.0f ? 0.0f : square * inverse;
    // 830BD350 is initialized by82F826F8 from82181A88=358637bd.
    return magnitude > rs::from_bits(0x358637bdu) ? scale(total, inverse) : ZERO;
}

}  // namespace s3::player::offboard::contact_toolkit
