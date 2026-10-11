// Port of skate-game/src/grind_world/derive.rs (s3 version).
//! Grind edges derived from triangle collision, for custom maps only. Stock
//! maps carry their own splines and never reach this. An edge qualifies when
//! it is a convex crease on a rideable face with a real drop beside it.
#include "s3/game/grind_world/derive.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <utility>

namespace s3::game::grind_world::derive {

namespace {

using P = std::array<double, 3>;

/// Same 1 mm weld as the collision world.
constexpr double WELD = 0.001;
/// Up component of a face the board can sit on.
constexpr double TOP_MIN_UP = 0.7;
/// Faces must turn by at least 40 degrees across the edge.
constexpr double MAX_FOLD_COS = -0.766;
/// Rise over length; steeper edges are posts, not rails.
constexpr double MAX_SLOPE = 0.7;
/// Where the ground beside the edge is sampled, and how far it must fall.
constexpr double PROBE_OUT = 0.12;
constexpr double PROBE_LIFT = 0.05;
constexpr double MIN_DROP = 0.25;
/// Clear air needed above the sample; less means the edge is buried in another solid.
constexpr double HEADROOM = 0.3;
constexpr double CHAIN_COS = 0.9;
constexpr double MIN_LENGTH = 0.6;
/// Both edges of a thin bar become one rail along its middle.
constexpr double MERGE_DISTANCE = 0.35;
constexpr double MERGE_HEIGHT = 0.05;
constexpr double EXISTING_DISTANCE = 0.3;
constexpr std::size_t MAX_RAILS = 20000;
/// The engine's cross-section test (S3 82C20728): short probes this far to each
/// side of the edge and this far above it. Surface on both sides, or anything
/// across the top, and the engine refuses the grind.
constexpr double SECTION_SIDE = 0.09;
constexpr double SECTION_HALF = 0.04;
/// Share of a line that may fail the cross-section test before it is dropped.
constexpr double MAX_BLOCKED = 0.5;
constexpr double CELL = 2.0;
constexpr std::int64_t MAX_CELLS = 256;

/// Rust `f64 as i64` (saturating, NaN -> 0).
std::int64_t as_i64(double x) {
    if (std::isnan(x)) return 0;
    if (x >= 9223372036854775807.0) return std::numeric_limits<std::int64_t>::max();
    if (x <= -9223372036854775808.0) return std::numeric_limits<std::int64_t>::min();
    return static_cast<std::int64_t>(x);
}
/// Rust `i64::saturating_mul`.
std::int64_t saturating_mul(std::int64_t a, std::int64_t b) {
    std::int64_t r;
    if (__builtin_mul_overflow(a, b, &r)) {
        return ((a < 0) != (b < 0)) ? std::numeric_limits<std::int64_t>::min()
                                    : std::numeric_limits<std::int64_t>::max();
    }
    return r;
}

P sub(P a, P b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(P a, P b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
P cross(P a, P b) {
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    };
}
double length(P a) { return std::sqrt(dot(a, a)); }
std::optional<P> unit(P a) {
    const double l = length(a);
    if (l > 1e-9) return P{a[0] / l, a[1] / l, a[2] / l};
    return std::nullopt;
}
P lerp(P a, P b, double t) {
    return {
        a[0] + (b[0] - a[0]) * t,
        a[1] + (b[1] - a[1]) * t,
        a[2] + (b[2] - a[2]) * t,
    };
}
P nearest_on_segment(P p, P a, P b) {
    const P ab = sub(b, a);
    const double l = dot(ab, ab);
    double t = 0.0;
    if (l > 0.0) {
        // `f64::clamp(0., 1.)`: NaN stays NaN.
        t = dot(sub(p, a), ab) / l;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
    }
    return lerp(a, b, t);
}
P nearest_on_line(P p, const std::vector<P>& line) {
    P best = line[0];
    double best_d = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i + 1 < line.size(); ++i) {
        const P q = nearest_on_segment(p, line[i], line[i + 1]);
        const double d = length(sub(p, q));
        if (d < best_d) {
            best_d = d;
            best = q;
        }
    }
    return best;
}
double line_length(const std::vector<P>& line) {
    double s = -0.0;
    for (std::size_t i = 0; i + 1 < line.size(); ++i) s += length(sub(line[i + 1], line[i]));
    return s;
}
/// Vertices and segment midpoints: enough to say one line hugs another.
std::vector<P> samples(const std::vector<P>& line) {
    std::vector<P> out(line.begin(), line.end());
    for (std::size_t i = 0; i + 1 < line.size(); ++i) out.push_back(lerp(line[i], line[i + 1], 0.5));
    return out;
}

using Cell = std::pair<std::int64_t, std::int64_t>;

/// Downward height lookups over the triangle soup.
struct Ground {
    const std::vector<std::array<P, 3>>& triangles;
    std::map<Cell, std::vector<std::uint32_t>> cells;
    std::vector<std::uint32_t> large;

    explicit Ground(const std::vector<std::array<P, 3>>& tris) : triangles(tris) {
        for (std::size_t i = 0; i < triangles.size(); ++i) {
            const auto& t = triangles[i];
            auto range = [&](std::size_t axis) {
                double lo = std::numeric_limits<double>::infinity();
                double hi = -std::numeric_limits<double>::infinity();
                for (const P& p : t) {
                    lo = std::fmin(lo, p[axis]);
                    hi = std::fmax(hi, p[axis]);
                }
                return std::make_pair(as_i64(std::floor(lo / CELL)), as_i64(std::floor(hi / CELL)));
            };
            const auto x = range(0);
            const auto z = range(2);
            if (saturating_mul(x.second - x.first + 1, z.second - z.first + 1) > MAX_CELLS) {
                large.push_back(static_cast<std::uint32_t>(i));
                continue;
            }
            for (std::int64_t cx = x.first; cx <= x.second; ++cx) {
                for (std::int64_t cz = z.first; cz <= z.second; ++cz) {
                    cells[{cx, cz}].push_back(static_cast<std::uint32_t>(i));
                }
            }
        }
    }

    const std::vector<std::uint32_t>& get(Cell c) const {
        static const std::vector<std::uint32_t> empty;
        const auto it = cells.find(c);
        return it == cells.end() ? empty : it->second;
    }

    /// Highest surface at or below `p` and lowest one above it, on its vertical.
    std::pair<std::optional<double>, std::optional<double>> around(P p) const {
        const Cell cell{as_i64(std::floor(p[0] / CELL)), as_i64(std::floor(p[2] / CELL))};
        std::optional<double> below;
        std::optional<double> above;
        const auto& near = get(cell);
        auto visit = [&](std::uint32_t i) {
            const auto& tri = triangles[i];
            const P a = tri[0], b = tri[1], c = tri[2];
            const double d = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2]);
            if (std::fabs(d) < 1e-12) return;
            const double u = ((b[2] - c[2]) * (p[0] - c[0]) + (c[0] - b[0]) * (p[2] - c[2])) / d;
            const double v = ((c[2] - a[2]) * (p[0] - c[0]) + (a[0] - c[0]) * (p[2] - c[2])) / d;
            const double w = 1.0 - u - v;
            if (u < -1e-9 || v < -1e-9 || w < -1e-9) return;
            const double y = u * a[1] + v * b[1] + w * c[1];
            if (y <= p[1]) {
                if (!below || y > *below) below = y;
            } else if (!above || y < *above) {
                above = y;
            }
        };
        for (const auto i : near) visit(i);
        for (const auto i : large) visit(i);
        return {below, above};
    }

    bool hits(P a, P b) const {
        auto cell = [](P p) { return Cell{as_i64(std::floor(p[0] / CELL)), as_i64(std::floor(p[2] / CELL))}; };
        const Cell first = cell(a);
        const Cell second = cell(b);
        const P delta = sub(b, a);
        auto test = [&](std::uint32_t i) {
            const auto& tri = triangles[i];
            const P p = tri[0], q = tri[1], r = tri[2];
            const P e1 = sub(q, p), e2 = sub(r, p);
            const P h = cross(delta, e2);
            const double det = dot(e1, h);
            if (std::fabs(det) < 1e-12) return false;
            const P s = sub(a, p);
            const double u = dot(s, h) / det;
            const P k = cross(s, e1);
            const double v = dot(delta, k) / det;
            const double t = dot(e2, k) / det;
            return u >= 0.0 && v >= 0.0 && u + v <= 1.0 && t >= 0.0 && t <= 1.0;
        };
        for (const auto i : get(first))
            if (test(i)) return true;
        if (second != first) {
            for (const auto i : get(second))
                if (test(i)) return true;
        }
        for (const auto i : large)
            if (test(i)) return true;
        return false;
    }

    /// Share of the line where the engine would see a blocked cross-section.
    double blocked(const std::vector<P>& line) const {
        int blocked_count = 0, total = 0;
        for (std::size_t s = 0; s + 1 < line.size(); ++s) {
            const P delta = sub(line[s + 1], line[s]);
            const auto direction = unit(delta);
            const auto up = unit(cross(delta, cross(P{0.0, 1.0, 0.0}, delta)));
            if (!direction || !up) continue;
            const P side = cross(*up, *direction);
            auto at = [](P p, P v, double d) { return P{p[0] + v[0] * d, p[1] + v[1] * d, p[2] + v[2] * d}; };
            for (const double t : {0.25, 0.5, 0.75}) {
                const P centre = lerp(line[s], line[s + 1], t);
                auto beside = [&](double d) {
                    const P p = at(centre, side, d);
                    return hits(at(p, *up, SECTION_HALF), at(p, *up, -SECTION_HALF));
                };
                const P over = at(centre, *up, SECTION_SIDE);
                total += 1;
                if ((beside(SECTION_SIDE) && beside(-SECTION_SIDE)) ||
                    hits(at(over, side, SECTION_SIDE), at(over, side, -SECTION_SIDE))) {
                    blocked_count += 1;
                }
            }
        }
        if (total == 0) return 1.0;
        return static_cast<double>(blocked_count) / static_cast<double>(total);
    }
};

}  // namespace

Derived rails(const std::vector<std::array<std::array<float, 3>, 3>>& collision,
              const std::vector<std::vector<std::array<float, 3>>>& own_in) {
    std::vector<std::array<P, 3>> triangles;
    triangles.reserve(collision.size());
    for (const auto& t : collision) {
        std::array<P, 3> tri;
        for (std::size_t i = 0; i < 3; ++i) {
            tri[i] = {static_cast<double>(t[i][0]), static_cast<double>(t[i][1]), static_cast<double>(t[i][2])};
        }
        triangles.push_back(tri);
    }
    // Welded ids in first-seen order keep the result the same on every run.
    std::map<std::array<std::int64_t, 3>, std::uint32_t> welded;
    std::vector<P> positions;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::size_t> edge_ids;
    // Per edge: the faces on it, each as (up-facing unit normal, third vertex).
    std::vector<std::pair<std::pair<std::uint32_t, std::uint32_t>, std::vector<std::pair<P, P>>>> edges;
    for (const auto& t : triangles) {
        auto normal_opt = unit(cross(sub(t[1], t[0]), sub(t[2], t[0])));
        if (!normal_opt) continue;
        P normal = *normal_opt;
        if (normal[1] < 0.0) normal = {-normal[0], -normal[1], -normal[2]};
        std::array<std::uint32_t, 3> ids;
        for (std::size_t k = 0; k < 3; ++k) {
            const P p = t[k];
            const std::array<std::int64_t, 3> key = {as_i64(std::round(p[0] / WELD)), as_i64(std::round(p[1] / WELD)),
                                                     as_i64(std::round(p[2] / WELD))};
            auto it = welded.find(key);
            if (it == welded.end()) {
                positions.push_back(p);
                it = welded.emplace(key, static_cast<std::uint32_t>(positions.size() - 1)).first;
            }
            ids[k] = it->second;
        }
        for (std::size_t i = 0; i < 3; ++i) {
            const std::uint32_t a = ids[i], b = ids[(i + 1) % 3];
            if (a == b) continue;
            const auto key = std::make_pair(std::min(a, b), std::max(a, b));
            auto it = edge_ids.find(key);
            if (it == edge_ids.end()) {
                edges.push_back({key, {}});
                it = edge_ids.emplace(key, edges.size() - 1).first;
            }
            edges[it->second].second.push_back({normal, t[(i + 2) % 3]});
        }
    }
    const Ground ground(triangles);
    std::vector<std::pair<std::uint32_t, std::uint32_t>> segments;
    for (const auto& [ab, faces] : edges) {
        const auto [a, b] = ab;
        if (faces.size() > 2) continue;
        const P pa = positions[a], pb = positions[b];
        const P along = sub(pb, pa);
        const auto axis_opt = unit(along);
        if (!axis_opt) continue;
        const P axis = *axis_opt;
        if (std::fabs(along[1]) > MAX_SLOPE * length(along)) continue;
        const std::size_t top = (faces.size() == 2 && faces[1].first[1] > faces[0].first[1]) ? 1 : 0;
        const P normal = faces[top].first;
        const P third = faces[top].second;
        if (normal[1] < TOP_MIN_UP) continue;
        // Direction across the edge, back into the top face.
        auto across = [&](P p) {
            const P v = sub(p, pa);
            const double al = dot(v, axis);
            return unit(sub(v, P{axis[0] * al, axis[1] * al, axis[2] * al}));
        };
        const auto inward_opt = across(third);
        if (!inward_opt) continue;
        const P inward = *inward_opt;
        if (faces.size() == 2) {
            const P other = faces[1 - top].second;
            const auto fold = across(other);
            if (!fold) continue;
            // The other face has to fall away from the top one, and sharply.
            if (dot(normal, sub(other, pa)) > -WELD || dot(inward, *fold) < MAX_FOLD_COS) continue;
        }
        const auto outward_opt = unit(P{-inward[0], 0.0, -inward[2]});
        if (!outward_opt) continue;
        const P outward = *outward_opt;
        const P mid = lerp(pa, pb, 0.5);
        const P probe = {mid[0] + outward[0] * PROBE_OUT, mid[1] + PROBE_LIFT, mid[2] + outward[2] * PROBE_OUT};
        const auto [below, above] = ground.around(probe);
        // Nothing below means the edge of the map, not a ledge.
        if (!below || mid[1] - *below < MIN_DROP || (above && *above - probe[1] < HEADROOM)) continue;
        segments.push_back({a, b});
    }

    // Join segments that continue each other into polylines.
    std::map<std::uint32_t, std::vector<std::size_t>> touching;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        touching[segments[i].first].push_back(i);
        touching[segments[i].second].push_back(i);
    }
    std::vector<bool> used(segments.size(), false);
    std::vector<std::vector<P>> lines;
    for (std::size_t start = 0; start < segments.size(); ++start) {
        if (used[start]) continue;
        used[start] = true;
        std::vector<std::uint32_t> chain = {segments[start].first, segments[start].second};
        for (int pass = 0; pass < 2; ++pass) {
            for (;;) {
                const std::uint32_t tip = chain[chain.size() - 1];
                if (tip == chain[0]) break;
                const P before = positions[chain[chain.size() - 2]];
                const auto heading = unit(sub(positions[tip], before));
                if (!heading) break;
                std::optional<std::pair<std::size_t, std::uint32_t>> next;
                int count = 0;
                for (const std::size_t j : touching.at(tip)) {
                    if (used[j]) continue;
                    const std::uint32_t far = segments[j].first == tip ? segments[j].second : segments[j].first;
                    const P step = sub(positions[far], positions[tip]);
                    const auto s = unit(step);
                    if (s && dot(*heading, *s) >= CHAIN_COS) {
                        count += 1;
                        next = std::make_pair(j, far);
                    }
                }
                if (!next || count != 1) break;
                used[next->first] = true;
                chain.push_back(next->second);
            }
            std::reverse(chain.begin(), chain.end());
        }
        std::vector<P> line;
        for (const std::uint32_t i : chain) {
            const P p = positions[i];
            // Straight runs need one segment, not one per triangle.
            if (line.size() >= 2) {
                const std::size_t n = line.size();
                const auto a = unit(sub(line[n - 1], line[n - 2]));
                const auto b = unit(sub(p, line[n - 1]));
                if (a && b && dot(*a, *b) > 0.9999) {
                    line[n - 1] = p;
                    continue;
                }
            }
            line.push_back(p);
        }
        if (line_length(line) >= MIN_LENGTH) lines.push_back(line);
    }

    std::vector<std::vector<P>> own;
    for (const auto& l : own_in) {
        std::vector<P> line;
        for (const auto& p : l) {
            line.push_back({static_cast<double>(p[0]), static_cast<double>(p[1]), static_cast<double>(p[2])});
        }
        own.push_back(line);
    }
    std::vector<bool> own_blocked;
    for (const auto& line : own) own_blocked.push_back(ground.blocked(line) > MAX_BLOCKED);
    std::vector<std::array<P, 2>> existing;
    for (std::size_t k = 0; k < own.size(); ++k) {
        if (own_blocked[k]) continue;
        for (std::size_t i = 0; i + 1 < own[k].size(); ++i) existing.push_back({own[k][i], own[k][i + 1]});
    }
    lines.erase(std::remove_if(lines.begin(), lines.end(),
                               [&](const std::vector<P>& line) {
                                   for (const P& p : samples(line)) {
                                       bool any = false;
                                       for (const auto& s : existing) {
                                           if (length(sub(p, nearest_on_segment(p, s[0], s[1]))) <=
                                               EXISTING_DISTANCE) {
                                               any = true;
                                               break;
                                           }
                                       }
                                       if (!any) return false;
                                   }
                                   return true;
                               }),
                lines.end());
    // Longest first, so a duplicate is always the shorter or equal line.
    std::stable_sort(lines.begin(), lines.end(), [](const std::vector<P>& a, const std::vector<P>& b) {
        // b.total_cmp(a) == Less  <=>  a should come first when len(a) > len(b) (total order).
        const double la = line_length(a), lb = line_length(b);
        auto key = [](double x) {
            std::int64_t bits;
            std::memcpy(&bits, &x, sizeof bits);
            return bits ^ static_cast<std::int64_t>(static_cast<std::uint64_t>(bits >> 63) >> 1);
        };
        return key(lb) < key(la);
    });
    std::vector<std::vector<P>> kept;
    for (auto& line : lines) {
        std::optional<std::size_t> twin;
        for (std::size_t k = 0; k < kept.size(); ++k) {
            bool all = true;
            for (const P& p : samples(line)) {
                const P q = nearest_on_line(p, kept[k]);
                if (!(std::fabs(p[1] - q[1]) <= MERGE_HEIGHT && length(sub(p, q)) <= MERGE_DISTANCE)) {
                    all = false;
                    break;
                }
            }
            if (all) {
                twin = k;
                break;
            }
        }
        if (twin) {
            const std::size_t k = *twin;
            if (line_length(line) >= 0.8 * line_length(kept[k])) {
                std::vector<P> centre;
                for (const P& p : kept[k]) centre.push_back(lerp(p, nearest_on_line(p, line), 0.5));
                // A bar too wide to grind down its middle keeps both edges.
                if (ground.blocked(centre) > MAX_BLOCKED) {
                    kept.push_back(line);
                } else {
                    kept[k] = centre;
                }
            }
        } else {
            kept.push_back(line);
        }
    }
    kept.erase(std::remove_if(kept.begin(), kept.end(),
                              [&](const std::vector<P>& line) { return !(ground.blocked(line) <= MAX_BLOCKED); }),
               kept.end());
    if (kept.size() > MAX_RAILS) kept.resize(MAX_RAILS);
    Derived out;
    out.own_blocked = own_blocked;
    for (const auto& line : kept) {
        std::vector<std::array<float, 3>> l;
        for (const P& p : line) {
            l.push_back({static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])});
        }
        out.rails.push_back(l);
    }
    return out;
}

}  // namespace s3::game::grind_world::derive
