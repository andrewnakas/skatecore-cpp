// Ported tests: skate-data gesture_patterns.rs and tests/input_config.rs.
#include "s3/data/gesture_patterns.hpp"
#include "test.hpp"

using namespace s3::data;

// gesture_patterns.rs: authored_order_and_per_pattern_tolerances_are_preserved
S3_TEST(gesture_authored_order_and_per_pattern_tolerances_are_preserved)
{
	auto p = gesture_patterns::parse(
	    "global_tolerance_dist 0.4\nglobal_anticipation_delay 15\npattern Ollie\ntolerance_dist 0.55\ncoord 0 1\ncoord 0 -1\npattern "
	    "Test\ncoord 1 0\ncoord -1 0");
	CHECK_OK(p);
	std::vector<std::array<float, 2>> expected = {{0.0f, 1.0f}, {0.0f, -1.0f}};
	CHECK(p.value[0].points == expected);
	CHECK_EQ(p.value[0].tolerance_squared, 0.55f * 0.55f);
	CHECK_EQ(p.value[1].tolerance_squared, 0.4f * 0.4f);
	CHECK(!gesture_patterns::parse("global_tolerance_dist 0.4\npattern Broken\ncoord 0 1").ok);
}

S3_TEST(gesture_rejects_unknown_directives_and_missing_tolerance)
{
	CHECK_ERR_CONTAINS(gesture_patterns::parse("pattern X\ncoord 0 1\ncoord 1 0"), "PAT lacks global_tolerance_dist");
	CHECK_ERR_CONTAINS(gesture_patterns::parse("global_tolerance_dist 0.4\npolar 1 2"), "invalid PAT directive at line 2: polar 1 2");
	CHECK_ERR_CONTAINS(gesture_patterns::parse("global_tolerance_dist 0x1"), "invalid PAT directive at line 1");
	CHECK_ERR_CONTAINS(gesture_patterns::parse("global_tolerance_dist inf"), "invalid PAT directive at line 1");
	// Comments and blank lines are ignored.
	auto ok = gesture_patterns::parse("// header\r\n\r\nglobal_tolerance_dist 0.5 // radius\npattern A\ncoord 0 1 // up\ncoord 0 -1\n");
	CHECK_OK(ok);
	CHECK_EQ(ok.value.size(), (size_t)1);
}

static std::string input_source()
{
	std::string out;
	for (size_t i = 0; i < 24; i++) out += std::string(input_config::BUTTON_NAMES[i]) + " = Button" + std::to_string(i) + ";\n";
	for (auto &g : input_config::GAMEPLAY) out += std::string(g[0]) + " = " + g[1] + ";\n";
	return out;
}

static std::string replace(std::string s, const std::string &from, const std::string &to)
{
	size_t at = s.find(from);
	if (at != std::string::npos) s.replace(at, from.size(), to);
	return s;
}

// tests/input_config.rs: accepts_spacing_comments_and_unrelated_menu_expressions
S3_TEST(input_config_accepts_spacing_comments_and_unrelated_menu_expressions)
{
	std::string source = "// controller mapping\n" + replace(input_source(), "LStickR-LStickL", "LStickR - LStickL // same expression\n") +
	                     "\nMenuKey = A.released ? 1 : -1;";
	auto r = StockGameplayConfig::validate(source);
	CHECK_OK(r);
}

// tests/input_config.rs: rejects_changed_axis_or_button_instead_of_using_incompatible_specialization
S3_TEST(input_config_rejects_changed_axis_or_button)
{
	std::string base = input_source();
	for (const std::string &changed : {replace(base, "GP_LStickX = LStickR-LStickL", "GP_LStickX = LStickL-LStickR"),
	                                   replace(base, "A = Button12", "A = Button13"), replace(base, "GP_AFace = A;", ""),
	                                   base + "GP_AFace = B;"}) {
		CHECK(!StockGameplayConfig::validate(changed).ok);
	}
}
