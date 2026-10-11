// VLT fields identified through Scorable82DA2340 and ScoreHolder82DA5D18 (skate-data scoring.rs,
// scoring_fields.rs). No fallback point values: a missing/invalid authored field is an error.
// The few skate-core types it needs (Scorable, catalog IDENTIFIERS, PointGraph<8>,
// skeleton_input::name::encode, session::Rules) are reproduced here in s3::data so the data
// stage stands alone; the core port should reuse or alias them.
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "collections.hpp"
#include "result.hpp"

namespace s3::data {

// skate_core::scoring::Scorable (id, fixed table +12 "class", +16 "score_type").
struct Scorable
{
	size_t id = 0;
	uint32_t class_ = 0;
	size_t score_type = 0;
};

// skate_core::animation::output::attributes::AttributeName
using AttributeName = std::array<uint32_t, 5>;
// skate_core::animation::skeleton_input::name::encode (FastString30 words).
AttributeName encode_attribute_name(const std::string &text);

// skate_core::point_graph::PointGraph<8> (TU3 0x82481E10 evaluator).
struct PointGraph8
{
	std::array<float, 8> x{};
	std::array<float, 8> y{};
	/// Native endpoint handling and slope-first binary32 interpolation.
	float evaluate(float input) const;
};

// skate_core::scoring::session::Rules
struct SessionRules
{
	float combo_capacity = 0;
	std::array<std::pair<float, float>, 3> combo_levels{};
	float combo_refresh_threshold = 0;
	float line_capacity = 0;
	float bail_factor = 0;
};

namespace scoring_tables {
struct ScorableIdentifier
{
	const char *name;
	uint32_t class_;
	size_t score_type;
};
// skate_core::scoring::catalog::IDENTIFIERS (332 entries, EScorableID order).
extern const ScorableIdentifier IDENTIFIERS[332];
struct CollectorField
{
	uint16_t offset;
	const char *field;
	size_t size;
};
// scoring_fields::FIELDS
extern const CollectorField FIELDS[62];
} // namespace scoring_tables

struct Definition
{
	Scorable metadata;
	const char *identifier = "";
	AttributeName encoded_name{};
	int32_t points = 0;
	std::string label;
	uint32_t trick_type = 0;
	float completion_delay = 0;
	int32_t variant = 0;
	uint32_t flags = 0;
};

class CollectorTuning
{
public:
	static Result<CollectorTuning> load(const Collections &data);
	float scalar(uint16_t native_offset) const { return scalars.at(native_offset); }
	float curve(uint16_t native_offset, float input) const { return curves.at(native_offset).evaluate(input); }

	std::map<uint16_t, float> scalars;
	std::map<uint16_t, PointGraph8> curves;
};

struct ScoringData
{
	CollectorTuning collector;
	std::vector<Definition> definitions;
	PointGraph8 repetition;
	PointGraph8 announcement;
	float line_drain = 0;
	float line_capacity = 0;
	float combo_drain = 0;
	float combo_capacity = 0;
	/// Ascending point thresholds and their authored multiplier.
	std::array<std::pair<float, float>, 3> combo_levels{};
	float combo_refresh_threshold = 0;
	float unannounced_factor = 0;
	float bail_factor = 0;
	float sketchy_side_speed = 0;

	SessionRules session_rules() const;
	static Result<ScoringData> load(const Collections &data);
	const Definition *by_name(const AttributeName &name) const;
	const Definition *by_id(size_t id) const;
};

// Setup's name dictionary may resolve the class as `scoring_trick`; presence checks use
// native identities (numeric names of scorable keys).
std::set<std::string> scorable_keys(const Collections &data);

} // namespace s3::data
