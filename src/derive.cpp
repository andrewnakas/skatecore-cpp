// Grind edges derived from triangle collision: port of skate-game `grind_world/derive.rs`.
// An edge qualifies when it is a convex crease on a rideable face with a real drop beside it.
// (The original also skips edges a map's own splines already cover; Halo maps have none.)
#include "world.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace skate {

namespace {

using P = std::array<double, 3>;

// Same 1 mm weld as the collision world.
constexpr double WELD = 0.001;
// Up component of a face the board can sit on.
constexpr double TOP_MIN_UP = 0.7;
// Faces must turn by at least 40 degrees across the edge.
constexpr double MAX_FOLD_COS = -0.766;
// Rise over length; steeper edges are posts, not rails.
constexpr double MAX_SLOPE = 0.7;
// Where the ground beside the edge is sampled, and how far it must fall.
constexpr double PROBE_OUT = 0.12;
constexpr double PROBE_LIFT = 0.05;
constexpr double MIN_DROP = 0.25;
// Clear air needed above the sample; less means the edge is buried in another solid.
constexpr double HEADROOM = 0.3;
constexpr double CHAIN_COS = 0.9;
constexpr double MIN_LENGTH = 0.6;
// Both edges of a thin bar become one rail along its middle.
constexpr double MERGE_DISTANCE = 0.35;
constexpr double MERGE_HEIGHT = 0.05;
constexpr size_t MAX_RAILS = 20000;
// The engine's cross-section test (S3 82C20728).
constexpr double SECTION_SIDE = 0.09;
constexpr double SECTION_HALF = 0.04;
constexpr double MAX_BLOCKED = 0.5;
constexpr double CELL = 2.0;
constexpr long long MAX_CELLS = 256;

P sub(P a, P b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(P a, P b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
P cross(P a, P b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double length(P a) { return std::sqrt(dot(a, a)); }
std::optional<P> unit(P a)
{
	double l = length(a);
	if (l > 1e-9)
		return P{a[0] / l, a[1] / l, a[2] / l};
	return std::nullopt;
}
P lerp(P a, P b, double t) { return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t}; }
P at(P p, P v, double d) { return {p[0] + v[0] * d, p[1] + v[1] * d, p[2] + v[2] * d}; }

P nearest_on_segment(P p, P a, P b)
{
	P ab = sub(b, a);
	double l = dot(ab, ab);
	double t = l > 0 ? std::clamp(dot(sub(p, a), ab) / l, 0.0, 1.0) : 0.0;
	return lerp(a, b, t);
}

P nearest_on_line(P p, const std::vector<P> &line)
{
	P best = line[0];
	double best_d = std::numeric_limits<double>::infinity();
	for (size_t i = 0; i + 1 < line.size(); ++i)
	{
		P q = nearest_on_segment(p, line[i], line[i + 1]);
		double d = length(sub(p, q));
		if (d < best_d)
		{
			best_d = d;
			best = q;
		}
	}
	return best;
}

double line_length(const std::vector<P> &line)
{
	double sum = 0;
	for (size_t i = 0; i + 1 < line.size(); ++i)
		sum += length(sub(line[i + 1], line[i]));
	return sum;
}

// Vertices and segment midpoints: enough to say one line hugs another.
std::vector<P> samples(const std::vector<P> &line)
{
	std::vector<P> out(line);
	for (size_t i = 0; i + 1 < line.size(); ++i)
		out.push_back(lerp(line[i], line[i + 1], 0.5));
	return out;
}

struct CellKey
{
	long long x, z;
	bool operator==(const CellKey &o) const { return x == o.x && z == o.z; }
};
struct CellHash
{
	size_t operator()(const CellKey &k) const { return std::hash<long long>()(k.x * 73856093LL ^ k.z * 19349663LL); }
};

// Downward height lookups over the triangle soup.
class Ground
{
public:
	explicit Ground(const std::vector<std::array<P, 3>> &triangles) : triangles_(triangles)
	{
		for (size_t i = 0; i < triangles.size(); ++i)
		{
			const auto &t = triangles[i];
			auto range = [&](int axis) {
				double lo = std::min({t[0][axis], t[1][axis], t[2][axis]});
				double hi = std::max({t[0][axis], t[1][axis], t[2][axis]});
				return std::pair<long long, long long>{(long long)std::floor(lo / CELL), (long long)std::floor(hi / CELL)};
			};
			auto x = range(0), z = range(2);
			if ((x.second - x.first + 1) * (z.second - z.first + 1) > MAX_CELLS)
			{
				large_.push_back((unsigned)i);
				continue;
			}
			for (long long cx = x.first; cx <= x.second; ++cx)
				for (long long cz = z.first; cz <= z.second; ++cz)
					cells_[{cx, cz}].push_back((unsigned)i);
		}
	}

	// Highest surface at or below p and lowest one above it, on its vertical.
	std::pair<std::optional<double>, std::optional<double>> around(P p) const
	{
		CellKey cell{(long long)std::floor(p[0] / CELL), (long long)std::floor(p[2] / CELL)};
		std::optional<double> below, above;
		auto visit = [&](unsigned i) {
			const auto &tri = triangles_[i];
			P a = tri[0], b = tri[1], c = tri[2];
			double d = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2]);
			if (std::fabs(d) < 1e-12)
				return;
			double u = ((b[2] - c[2]) * (p[0] - c[0]) + (c[0] - b[0]) * (p[2] - c[2])) / d;
			double v = ((c[2] - a[2]) * (p[0] - c[0]) + (a[0] - c[0]) * (p[2] - c[2])) / d;
			double w = 1 - u - v;
			if (u < -1e-9 || v < -1e-9 || w < -1e-9)
				return;
			double y = u * a[1] + v * b[1] + w * c[1];
			if (y <= p[1])
			{
				if (!below || y > *below)
					below = y;
			}
			else if (!above || y < *above)
				above = y;
		};
		auto it = cells_.find(cell);
		if (it != cells_.end())
			for (unsigned i : it->second)
				visit(i);
		for (unsigned i : large_)
			visit(i);
		return {below, above};
	}

	bool hits(P a, P b) const
	{
		auto cell = [](P p) { return CellKey{(long long)std::floor(p[0] / CELL), (long long)std::floor(p[2] / CELL)}; };
		CellKey first = cell(a), second = cell(b);
		P delta = sub(b, a);
		auto test = [&](unsigned i) {
			const auto &tri = triangles_[i];
			P e1 = sub(tri[1], tri[0]), e2 = sub(tri[2], tri[0]);
			P h = cross(delta, e2);
			double det = dot(e1, h);
			if (std::fabs(det) < 1e-12)
				return false;
			P s = sub(a, tri[0]);
			double u = dot(s, h) / det;
			P k = cross(s, e1);
			double v = dot(delta, k) / det;
			double t = dot(e2, k) / det;
			return u >= 0 && v >= 0 && u + v <= 1 && t >= 0 && t <= 1;
		};
		auto scan = [&](CellKey c) {
			auto it = cells_.find(c);
			if (it != cells_.end())
				for (unsigned i : it->second)
					if (test(i))
						return true;
			return false;
		};
		if (scan(first))
			return true;
		if (!(second == first) && scan(second))
			return true;
		for (unsigned i : large_)
			if (test(i))
				return true;
		return false;
	}

	// Share of the line where the engine would see a blocked cross-section.
	double blocked(const std::vector<P> &line) const
	{
		int blocked = 0, total = 0;
		for (size_t i = 0; i + 1 < line.size(); ++i)
		{
			P delta = sub(line[i + 1], line[i]);
			auto direction = unit(delta);
			auto up = unit(cross(delta, cross(P{0, 1, 0}, delta)));
			if (!direction || !up)
				continue;
			P side = cross(*up, *direction);
			for (double t : {0.25, 0.5, 0.75})
			{
				P centre = lerp(line[i], line[i + 1], t);
				auto beside = [&](double d) {
					P p = at(centre, side, d);
					return hits(at(p, *up, SECTION_HALF), at(p, *up, -SECTION_HALF));
				};
				P over = at(centre, *up, SECTION_SIDE);
				total += 1;
				if ((beside(SECTION_SIDE) && beside(-SECTION_SIDE)) ||
					hits(at(over, side, SECTION_SIDE), at(over, side, -SECTION_SIDE)))
					blocked += 1;
			}
		}
		return total == 0 ? 1.0 : (double)blocked / (double)total;
	}

private:
	const std::vector<std::array<P, 3>> &triangles_;
	std::unordered_map<CellKey, std::vector<unsigned>, CellHash> cells_;
	std::vector<unsigned> large_;
};

struct WeldKey
{
	long long x, y, z;
	bool operator==(const WeldKey &o) const { return x == o.x && y == o.y && z == o.z; }
};
struct WeldHash
{
	size_t operator()(const WeldKey &k) const
	{
		return std::hash<long long>()(k.x * 73856093LL ^ k.y * 19349663LL ^ k.z * 83492791LL);
	}
};

} // namespace

std::vector<std::vector<Vec3>> derive_rail_lines(const std::vector<std::array<std::array<float, 3>, 3>> &collision)
{
	std::vector<std::array<P, 3>> triangles;
	triangles.reserve(collision.size());
	for (const auto &t : collision)
		triangles.push_back({P{t[0][0], t[0][1], t[0][2]}, P{t[1][0], t[1][1], t[1][2]}, P{t[2][0], t[2][1], t[2][2]}});

	// Welded ids in first-seen order keep the result the same on every run.
	std::unordered_map<WeldKey, unsigned, WeldHash> welded;
	std::vector<P> positions;
	std::map<std::pair<unsigned, unsigned>, size_t> edge_ids;
	// Per edge: the faces on it, each as (up-facing unit normal, third vertex).
	std::vector<std::pair<std::pair<unsigned, unsigned>, std::vector<std::pair<P, P>>>> edges;
	for (const auto &t : triangles)
	{
		auto n = unit(cross(sub(t[1], t[0]), sub(t[2], t[0])));
		if (!n)
			continue;
		P normal = *n;
		if (normal[1] < 0)
			normal = {-normal[0], -normal[1], -normal[2]};
		unsigned ids[3];
		for (int k = 0; k < 3; ++k)
		{
			WeldKey key{(long long)std::llround(t[k][0] / WELD), (long long)std::llround(t[k][1] / WELD),
				(long long)std::llround(t[k][2] / WELD)};
			auto it = welded.find(key);
			if (it == welded.end())
			{
				positions.push_back(t[k]);
				it = welded.emplace(key, (unsigned)(positions.size() - 1)).first;
			}
			ids[k] = it->second;
		}
		for (int i = 0; i < 3; ++i)
		{
			unsigned a = ids[i], b = ids[(i + 1) % 3];
			if (a == b)
				continue;
			std::pair<unsigned, unsigned> key{std::min(a, b), std::max(a, b)};
			auto it = edge_ids.find(key);
			if (it == edge_ids.end())
			{
				edges.push_back({key, {}});
				it = edge_ids.emplace(key, edges.size() - 1).first;
			}
			edges[it->second].second.push_back({normal, t[(i + 2) % 3]});
		}
	}

	Ground ground(triangles);
	std::vector<std::pair<unsigned, unsigned>> segments;
	for (const auto &edge : edges)
	{
		unsigned a = edge.first.first, b = edge.first.second;
		const auto &faces = edge.second;
		if (faces.size() > 2)
			continue;
		P pa = positions[a], pb = positions[b];
		P along = sub(pb, pa);
		auto axis_opt = unit(along);
		if (!axis_opt)
			continue;
		P axis = *axis_opt;
		if (std::fabs(along[1]) > MAX_SLOPE * length(along))
			continue;
		size_t top = faces.size() == 2 && faces[1].first[1] > faces[0].first[1] ? 1 : 0;
		P normal = faces[top].first, third = faces[top].second;
		if (normal[1] < TOP_MIN_UP)
			continue;
		// Direction across the edge, back into the top face.
		auto across = [&](P p) {
			P v = sub(p, pa);
			double l = dot(v, axis);
			return unit(sub(v, P{axis[0] * l, axis[1] * l, axis[2] * l}));
		};
		auto inward = across(third);
		if (!inward)
			continue;
		if (faces.size() == 2)
		{
			P other = faces[1 - top].second;
			auto fold = across(other);
			if (!fold)
				continue;
			// The other face has to fall away from the top one, and sharply.
			if (dot(normal, sub(other, pa)) > -WELD || dot(*inward, *fold) < MAX_FOLD_COS)
				continue;
		}
		auto outward = unit(P{-(*inward)[0], 0, -(*inward)[2]});
		if (!outward)
			continue;
		P mid = lerp(pa, pb, 0.5);
		P probe{mid[0] + (*outward)[0] * PROBE_OUT, mid[1] + PROBE_LIFT, mid[2] + (*outward)[2] * PROBE_OUT};
		auto [below, above] = ground.around(probe);
		// Nothing below means the edge of the map, not a ledge.
		if (!below || mid[1] - *below < MIN_DROP || (above && *above - probe[1] < HEADROOM))
			continue;
		segments.push_back({a, b});
	}

	// Join segments that continue each other into polylines.
	std::unordered_map<unsigned, std::vector<size_t>> touching;
	for (size_t i = 0; i < segments.size(); ++i)
	{
		touching[segments[i].first].push_back(i);
		touching[segments[i].second].push_back(i);
	}
	std::vector<bool> used(segments.size(), false);
	std::vector<std::vector<P>> lines;
	for (size_t start = 0; start < segments.size(); ++start)
	{
		if (used[start])
			continue;
		used[start] = true;
		std::vector<unsigned> chain{segments[start].first, segments[start].second};
		for (int pass = 0; pass < 2; ++pass)
		{
			for (;;)
			{
				unsigned tip = chain.back();
				if (tip == chain.front())
					break;
				P before = positions[chain[chain.size() - 2]];
				auto heading = unit(sub(positions[tip], before));
				if (!heading)
					break;
				std::optional<std::pair<size_t, unsigned>> next;
				int count = 0;
				for (size_t j : touching[tip])
				{
					if (used[j])
						continue;
					unsigned far = segments[j].first == tip ? segments[j].second : segments[j].first;
					auto step = unit(sub(positions[far], positions[tip]));
					if (step && dot(*heading, *step) >= CHAIN_COS)
					{
						count += 1;
						next = std::pair<size_t, unsigned>{j, far};
					}
				}
				if (!next || count != 1)
					break;
				used[next->first] = true;
				chain.push_back(next->second);
			}
			std::reverse(chain.begin(), chain.end());
		}
		std::vector<P> line;
		for (unsigned id : chain)
		{
			P p = positions[id];
			// Straight runs need one segment, not one per triangle.
			if (line.size() >= 2)
			{
				size_t n = line.size();
				auto u = unit(sub(line[n - 1], line[n - 2])), w = unit(sub(p, line[n - 1]));
				if (u && w && dot(*u, *w) > 0.9999)
				{
					line[n - 1] = p;
					continue;
				}
			}
			line.push_back(p);
		}
		if (line_length(line) >= MIN_LENGTH)
			lines.push_back(line);
	}

	// Longest first, so a duplicate is always the shorter or equal line.
	std::stable_sort(lines.begin(), lines.end(),
		[](const std::vector<P> &a, const std::vector<P> &b) { return line_length(b) < line_length(a); });
	std::vector<std::vector<P>> kept;
	for (auto &line : lines)
	{
		std::optional<size_t> twin;
		auto line_samples = samples(line);
		for (size_t k = 0; k < kept.size() && !twin; ++k)
		{
			bool all = true;
			for (const P &p : line_samples)
			{
				P q = nearest_on_line(p, kept[k]);
				if (!(std::fabs(p[1] - q[1]) <= MERGE_HEIGHT && length(sub(p, q)) <= MERGE_DISTANCE))
				{
					all = false;
					break;
				}
			}
			if (all)
				twin = k;
		}
		if (twin && line_length(line) >= 0.8 * line_length(kept[*twin]))
		{
			std::vector<P> centre;
			for (const P &p : kept[*twin])
				centre.push_back(lerp(p, nearest_on_line(p, line), 0.5));
			// A bar too wide to grind down its middle keeps both edges.
			if (ground.blocked(centre) > MAX_BLOCKED)
				kept.push_back(line);
			else
				kept[*twin] = centre;
		}
		else if (!twin)
		{
			kept.push_back(line);
		}
	}
	std::vector<std::vector<Vec3>> out;
	for (const auto &line : kept)
	{
		if (ground.blocked(line) > MAX_BLOCKED)
			continue;
		std::vector<Vec3> rail;
		for (const P &p : line)
			rail.push_back({(float)p[0], (float)p[1], (float)p[2]});
		out.push_back(std::move(rail));
		if (out.size() >= MAX_RAILS)
			break;
	}
	return out;
}

size_t World::derive_rails(const float *vertices, int vertex_count, const int *indices, int triangle_count)
{
	std::vector<std::array<std::array<float, 3>, 3>> soup;
	soup.reserve((size_t)triangle_count);
	for (int t = 0; t < triangle_count; ++t)
	{
		std::array<std::array<float, 3>, 3> tri;
		bool ok = true;
		for (int k = 0; k < 3; ++k)
		{
			int i = indices[t * 3 + k];
			if (i < 0 || i >= vertex_count)
			{
				ok = false;
				break;
			}
			tri[k] = {vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]};
		}
		if (ok)
			soup.push_back(tri);
	}
	auto lines = derive_rail_lines(soup);
	for (auto &line : lines)
		add_rail(std::move(line));
	return lines.size();
}

} // namespace skate
