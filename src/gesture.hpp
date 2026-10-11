// Flick-it gesture recognizer: line-for-line port of skate-core `input/gesture.rs`
// (TU3 pattern recognition, 82697168/826972B8/826974A8).
// Points are supplied in PAT authoring order.
#pragma once
#include <optional>
#include <string>
#include <vector>

namespace skate {

struct Pattern
{
	std::string name;
	std::vector<std::pair<float, float>> points;
	float tolerance_squared;
};

struct GestureSettings
{
	// Recognizer config byte 33. Supplied by the owner, never inferred here.
	unsigned char maximum_misses;
	unsigned difficulty;
};

struct Recognition
{
	size_t pattern;
	float strength;
	float distance;
	float elapsed;
};

class Recognizer
{
public:
	// Returns false (and leaves the recognizer empty) when a pattern is invalid.
	bool init(std::vector<Pattern> patterns, std::string *error = nullptr);
	const std::vector<Pattern> &patterns() const { return patterns_; }
	// Held messages are checked before recognition in 826962D8.
	std::optional<size_t> held(std::pair<float, float> sample);
	std::optional<Recognition> sample(std::pair<float, float> sample, GestureSettings settings);
	void reset();

private:
	struct Node
	{
		bool active = false;
		bool complete = false;
		size_t next = 0;
		unsigned short elapsed = 0;
		unsigned char misses = 0;
		float distance = 0.0f;
		void tick(const Pattern &pattern, std::pair<float, float> sample, unsigned char maximum_misses);
		float score(size_t count) const;
	};

	std::vector<Pattern> patterns_;
	std::vector<Node> nodes_;
	bool has_previous_sample_ = false;
	bool refractory_ = false;
	std::optional<size_t> held_;
};

// Parser for the stock `.pat` text format (skate_data::gesture_patterns::parse):
// `global_tolerance_dist d`, `pattern Name`, `tolerance_dist d`, `coord x y`, `//` comments.
std::vector<Pattern> parse_patterns(const char *text);

// The authored pattern set shipped with the library (the stock .pat files are not
// redistributable). Stick space after the native Y negation: +x right, +y = stick DOWN.
const char *authored_right_stick_patterns();

} // namespace skate
