// Port of skate-core `input/gesture.rs`. Comments carry the original addresses.
#include "gesture.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace skate {

static float distance_squared(std::pair<float, float> a, std::pair<float, float> b)
{
	float x = a.first - b.first;
	float y = a.second - b.second;
	// VMX multiplies then adds; do not contract to FMA.
	volatile float xx = x * x;
	volatile float yy = y * y;
	return xx + yy;
}

void Recognizer::Node::tick(const Pattern &pattern, std::pair<float, float> sample, unsigned char maximum_misses)
{
	if (!active)
	{
		float d = distance_squared(pattern.points[0], sample);
		if (d <= pattern.tolerance_squared)
		{
			*this = Node{};
			active = true;
			next = 1;
			elapsed = 1;
			distance = d;
		}
	}
	else if (!complete)
	{
		float d = distance_squared(pattern.points[next], sample);
		if (d <= pattern.tolerance_squared)
		{
			distance += d;
			elapsed = (unsigned short)((elapsed + 1) & 0x3ff);
			if (next + 1 == pattern.points.size())
			{
				complete = true;
			}
			else
			{
				next += 1;
				misses = 0;
			}
		}
		else
		{
			// Holding the first point does not age a pending gesture.
			if (next != 1 || distance_squared(pattern.points[0], sample) > pattern.tolerance_squared)
			{
				elapsed = (unsigned short)((elapsed + 1) & 0x3ff);
				misses = (unsigned char)((misses + 1) & 0x3f);
			}
			if (misses > maximum_misses)
				*this = Node{};
		}
	}
}

float Recognizer::Node::score(size_t count_) const
{
	float count = (float)count_;
	// The unusual two-sided 0.15 operation is present in 826972B8.
	float mean = std::fmin(std::fmax(distance, 0.15f) / count, 0.15f);
	return (count * count * count * count) / (mean * (float)elapsed);
}

bool Recognizer::init(std::vector<Pattern> patterns, std::string *error)
{
	for (const Pattern &p : patterns)
	{
		bool bad = p.points.size() < 2 || p.points.size() > 15 || !std::isfinite(p.tolerance_squared) ||
			p.tolerance_squared < 0.0f;
		for (auto &pt : p.points)
			bad = bad || !std::isfinite(pt.first) || !std::isfinite(pt.second);
		if (bad)
		{
			if (error)
				*error = "invalid native gesture pattern " + p.name;
			patterns_.clear();
			nodes_.clear();
			return false;
		}
	}
	patterns_ = std::move(patterns);
	nodes_.assign(patterns_.size(), Node{});
	has_previous_sample_ = false;
	refractory_ = false;
	held_.reset();
	return true;
}

void Recognizer::reset()
{
	nodes_.assign(patterns_.size(), Node{});
	has_previous_sample_ = false;
	refractory_ = false;
	held_.reset();
}

// Native point zero is the final authored coordinate (82699738 push-front).
std::optional<size_t> Recognizer::held(std::pair<float, float> sample)
{
	if (held_)
	{
		const Pattern &pattern = patterns_[*held_];
		if (distance_squared(pattern.points.back(), sample) > pattern.tolerance_squared)
			held_.reset();
	}
	return held_;
}

std::optional<Recognition> Recognizer::sample(std::pair<float, float> s, GestureSettings settings)
{
	if (refractory_)
	{
		nodes_.assign(patterns_.size(), Node{});
		refractory_ = false;
		return std::nullopt;
	}
	if (!has_previous_sample_)
	{
		has_previous_sample_ = true;
		return std::nullopt;
	}
	std::optional<size_t> best;
	for (size_t i = 0; i < nodes_.size(); ++i)
		nodes_[i].tick(patterns_[i], s, settings.maximum_misses);
	for (size_t i = 0; i < nodes_.size(); ++i)
	{
		if (nodes_[i].complete &&
			(!best || nodes_[i].score(patterns_[i].points.size()) > nodes_[*best].score(patterns_[*best].points.size())))
		{
			best = i;
		}
	}
	if (!best)
		return std::nullopt;
	size_t pattern = *best;
	Node node = nodes_[pattern];
	float ratio = (float)node.elapsed / (float)patterns_[pattern].points.size();
	// 822249B4/82063B08 and 821E63E8/821E63EC, checked against TU3.
	float low = settings.difficulty == 2 ? 1.5f : 1.75f;
	float high = settings.difficulty == 2 ? 3.0f : 4.4f;
	float strength;
	if (ratio <= low)
		strength = 1.0f;
	else if (ratio >= high)
		strength = 0.0f;
	else
	{
		float slope = 1.0f / (low - high);
		strength = std::fma(slope, ratio, -(slope * high));
	}
	refractory_ = true;
	held_ = pattern;
	return Recognition{pattern, strength, node.distance, (float)node.elapsed};
}

std::vector<Pattern> parse_patterns(const char *text)
{
	std::vector<Pattern> out;
	float global = 0.4f;
	std::istringstream in(text);
	std::string line;
	while (std::getline(in, line))
	{
		size_t c = line.find("//");
		if (c != std::string::npos)
			line.resize(c);
		std::istringstream ls(line);
		std::string word;
		if (!(ls >> word))
			continue;
		if (word == "global_tolerance_dist")
			ls >> global;
		else if (word == "pattern")
		{
			Pattern p;
			ls >> p.name;
			p.tolerance_squared = global * global;
			out.push_back(p);
		}
		else if (word == "tolerance_dist" && !out.empty())
		{
			float d;
			ls >> d;
			out.back().tolerance_squared = d * d;
		}
		else if (word == "coord" && !out.empty())
		{
			float x, y;
			ls >> x >> y;
			out.back().points.push_back({x, y});
		}
		// time / speed directives are ignored by the native parser too.
	}
	return out;
}

// Authored for this port (the stock skater.pat is retail data). Shapes follow the
// Skate 3 manual: pull back then flick for ollies and flips, scoops for shove-its,
// and the 360 flip uses the exact points of skate-core's recovered test vector.
// +y is stick DOWN (native negation). No pattern is a prefix of another.
const char *authored_right_stick_patterns()
{
	return
		"global_tolerance_dist 0.4\n"
		"pattern Ollie\n coord 0 1\n coord 0 -1\n"
		"pattern Nollie\n coord 0 -1\n coord 0 1\n"
		"pattern Kickflip\n coord 0 1\n coord -0.707 -0.707\n"
		"pattern Heelflip\n coord 0 1\n coord 0.707 -0.707\n"
		"pattern PopShuvit\n coord 0 1\n coord -0.707 0.707\n coord -1 0\n"
		"pattern FSPopShuvit\n coord 0 1\n coord 0.707 0.707\n coord 1 0\n"
		"pattern 360Flip\n coord -0.965714 0.268571\n coord -0.497143 0.84\n coord 0.211429 0.988571\n coord 0.908571 -0.405714\n"
		"pattern Laserflip\n coord 0.965714 0.268571\n coord 0.497143 0.84\n coord -0.211429 0.988571\n coord -0.908571 -0.405714\n"
		"pattern VarialKickflip\n coord -0.707 0.707\n coord 0.707 -0.707\n"
		"pattern VarialHeelflip\n coord 0.707 0.707\n coord -0.707 -0.707\n"
		"pattern Hardflip\n coord -1 0\n coord 1 0\n"
		"pattern InwardHeelflip\n coord 1 0\n coord -1 0\n"
		"pattern N_Kickflip\n coord 0 -1\n coord -0.707 0.707\n"
		"pattern N_Heelflip\n coord 0 -1\n coord 0.707 0.707\n"
		"pattern N_PopShuvit\n coord 0 -1\n coord -0.707 -0.707\n coord -1 0\n"
		"pattern N_FSPopShuvit\n coord 0 -1\n coord 0.707 -0.707\n coord 1 0\n";
}

} // namespace skate
