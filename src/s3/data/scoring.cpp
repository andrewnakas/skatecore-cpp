// Scoring VLT data (scoring.rs) plus the small skate-core pieces it depends on.
#include "scoring.hpp"

#include <cmath>

#include "attrib_hash.hpp"

namespace s3::data {

static const char *const SCORABLE = "Hash_6918469984A8C596";
static const char *const TUNING = "Hash_349215E2E817703C";

AttributeName encode_attribute_name(const std::string &text)
{
	AttributeName words{};
	size_t position = 0;
	while (position < 30 && position < text.size() && text[position] != 0) {
		uint32_t weight = 79235168u; // 38^5, exactly the native first weight.
		size_t word = position / 6;
		int count = 0;
		while (count < 6 && position < text.size() && text[position] != 0) {
			int32_t byte = (int32_t)(int8_t)text[position];
			int32_t digit = byte >= 97 ? byte - 86 : byte > 90 ? byte - 58 : byte > 57 ? byte - 54 : byte - 47;
			words[word] = words[word] + (uint32_t)digit * weight;
			weight /= 38;
			count++;
			position++;
		}
	}
	return words;
}

float PointGraph8::evaluate(float input) const
{
	const size_t N = 8;
	if (input < x[0]) return y[0];
	// The native branch enters the scan only on an ordered less-than.
	// Unordered input or a NaN final key therefore selects the last value.
	if (!(input < x[N - 1])) return y[N - 1];
	for (size_t upper = 1; upper < N; upper++) {
		if (!(input < x[upper])) continue;
		size_t lower = upper - 1;
		float width = x[upper] - x[lower];
		if (width <= 0.0f) return y[upper];
		float slope = (y[upper] - y[lower]) / width;
		return std::fma(slope, input - x[lower], y[lower]);
	}
	return y[0];
}

std::set<std::string> scorable_keys(const Collections &data)
{
	std::set<std::string> out;
	for (const Collection &row : data.entries())
		if (row.class_id == SCORABLE) out.insert(row.key_id);
	return out;
}

SessionRules ScoringData::session_rules() const
{
	return SessionRules{combo_capacity, combo_levels, combo_refresh_threshold, line_capacity, bail_factor};
}

static PointGraph8 graph_from_words(const std::array<uint32_t, 20> &w)
{
	PointGraph8 g;
	for (size_t i = 0; i < 8; i++) {
		g.x[i] = f32_from_bits(w[4 + i]);
		g.y[i] = f32_from_bits(w[12 + i]);
	}
	return g;
}

Result<CollectorTuning> CollectorTuning::load(const Collections &data)
{
	CollectorTuning out;
	for (const auto &f : scoring_tables::FIELDS) {
		if (f.size == 4) {
			S3_TRY(float v, data.float_("Hash_546C36B656038E04", "default", f.field));
			out.scalars[f.offset] = v;
		} else {
			S3_TRY(auto w, data.words<20>("Hash_546C36B656038E04", "default", f.field));
			out.curves[f.offset] = graph_from_words(w);
		}
	}
	return out;
}

Result<ScoringData> ScoringData::load(const Collections &data)
{
	ScoringData out;
	std::set<std::string> available = scorable_keys(data);
	for (size_t id = 0; id < 332; id++) {
		const auto &entry = scoring_tables::IDENTIFIERS[id];
		const std::string identifier = entry.name;
		// The executable enum includes unused entries without VLT records.
		// Only actual records are eligible for recognition.
		if (!available.count(attrib_hash::numeric_name(identifier))) continue;
		auto words = [&](const char *name) -> Result<uint32_t> {
			S3_TRY(auto w, data.words<1>(SCORABLE, identifier, name));
			return w[0];
		};
		S3_TRY(uint32_t actual_id, words("Hash_B2383F16252DFE8E"));
		if (actual_id != (uint32_t)id)
			return Error{"Scorable " + identifier + ": enum " + std::to_string(id) + " disagrees with VLT " + std::to_string(actual_id)};
		S3_TRY(const Field *label, data.field(SCORABLE, identifier, "Hash_843613E915014627"));
		if (label->type_name != "EA::Reflection::Text") return Error{"Invalid scorable label " + identifier};
		Definition d;
		d.metadata = Scorable{id, entry.class_, entry.score_type};
		d.identifier = entry.name;
		d.encoded_name = encode_attribute_name(identifier);
		S3_TRY(uint32_t points, data.integer(SCORABLE, identifier, "Hash_937F62AE5C1ED284"));
		d.points = (int32_t)points;
		d.label = label->data;
		S3_TRY(d.trick_type, words("TrickType"));
		S3_TRY(d.completion_delay, data.float_(SCORABLE, identifier, "Hash_DC7F402E5680BA02"));
		S3_TRY(uint32_t variant, words("Hash_2E90BC04042A0B5A"));
		d.variant = (int32_t)variant;
		S3_TRY(d.flags, words("Hash_D34A84B044B60CE3"));
		out.definitions.push_back(std::move(d));
	}
	if (out.definitions.empty()) return Error{"No native scorable definitions in owned VLT data"};
	auto graph = [&](const char *field) -> Result<PointGraph8> {
		S3_TRY(auto w, data.words<20>(TUNING, "default", field));
		PointGraph8 g = graph_from_words(w);
		bool bad = false;
		for (size_t i = 0; i < 8; i++) bad |= !is_finite(g.x[i]) || !is_finite(g.y[i]);
		for (size_t i = 0; i + 1 < 8; i++) bad |= g.x[i] > g.x[i + 1];
		if (bad) return Error{std::string("Invalid native scoring curve ") + field};
		return g;
	};
	auto f = [&](const char *field) { return data.float_(TUNING, "default", field); };
	S3_TRY(out.collector, CollectorTuning::load(data));
	S3_TRY(out.repetition, graph("Hash_59D91EAABF033A24"));
	S3_TRY(out.announcement, graph("Hash_263B277F8CA17126"));
	S3_TRY(out.line_drain, f("Hash_FE02A45231F7B06A"));
	S3_TRY(out.line_capacity, f("Hash_57478337ACCFFF18"));
	S3_TRY(out.combo_drain, f("Hash_88407506DC9780ED"));
	S3_TRY(out.combo_capacity, f("Hash_28767A5F961C7129"));
	S3_TRY(out.combo_levels[0].first, f("Hash_829887D8C24A5B8A"));
	S3_TRY(out.combo_levels[0].second, f("Hash_4752056364FF91FE"));
	S3_TRY(out.combo_levels[1].first, f("Hash_FB8408A15C17A9D4"));
	S3_TRY(out.combo_levels[1].second, f("Hash_F4993A13ED7B44C1"));
	S3_TRY(out.combo_levels[2].first, f("Hash_30AAE071A7868771"));
	S3_TRY(out.combo_levels[2].second, f("Hash_DC0C3853F6C5FC2E"));
	S3_TRY(out.combo_refresh_threshold, f("Hash_E36197BFC8EA1CAD"));
	S3_TRY(out.unannounced_factor, f("Hash_2577DF0FCE5251E4"));
	S3_TRY(out.bail_factor, f("Hash_B0B56FF046508506"));
	S3_TRY(out.sketchy_side_speed, f("Hash_6DC5982591F80A4C"));
	return out;
}

const Definition *ScoringData::by_name(const AttributeName &name) const
{
	for (const Definition &d : definitions)
		if (d.encoded_name == name) return &d;
	return nullptr;
}

const Definition *ScoringData::by_id(size_t id) const
{
	for (const Definition &d : definitions)
		if (d.metadata.id == id) return &d;
	return nullptr;
}

} // namespace s3::data
