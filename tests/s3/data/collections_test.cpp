// Ported tests: skate-data collections.rs, attrib_hash, sha256, scoring.rs identity test,
// skate-core scoring catalog metric ids; plus JSON parser checks.
#include <cmath>

#include "s3/data/attrib_hash.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/json.hpp"
#include "s3/data/scoring.hpp"
#include "test.hpp"

using namespace s3::data;

// collections.rs: numeric_export_names_resolve_readable_manual_settings
S3_TEST(collections_numeric_export_names_resolve_readable_manual_settings)
{
	// Identities and payload from the prepared vault; no asset IO or gameplay.
	std::string json = R"({"version": 1, "collections": [{
		"class": "anim_motion", "key": ")" + attrib_hash::numeric_name("manual") + R"(",
		"parent": "", "source": "fixture", "sha256": "",
		"fields": { "Hash_A353CE1670D3AA40": { "type": "EA::Reflection::Float", "data": "3D23D70A" } }
	}]})";
	auto data = Collections::from_json(json);
	CHECK_OK(data);
	auto v = data->float_("anim_motion", "manual", "manual_clamp_vel");
	CHECK_OK(v);
	CHECK_EQ(f32_to_bits(v.value), 0x3d23d70au);
	CHECK(!data->field("anim_motion", "manual", "missing").ok);
}

// collections.rs: readable_names_resolve_numeric_queries_through_inheritance
S3_TEST(collections_readable_names_resolve_numeric_queries_through_inheritance)
{
	std::string json = R"({"version": 1, "collections": [
		{"class":"example", "key":"child", "parent":")" + attrib_hash::numeric_name("base") + R"(",
		 "source":"fixture", "sha256":"", "fields":{}},
		{"class":"example", "key":"base", "parent":"", "source":"fixture", "sha256":"",
		 "fields":{"value":{"type":"EA::Reflection::Float", "data":"3F800000"}}}
	]})";
	auto data = Collections::from_json(json);
	CHECK_OK(data);
	auto v = data->float_(attrib_hash::numeric_name("example"), attrib_hash::numeric_name("child"), attrib_hash::numeric_name("value"));
	CHECK_OK(v);
	CHECK_EQ(v.value, 1.0f);
}

// collections.rs: float_arrays_read_their_items_not_the_header_lanes
S3_TEST(collections_float_arrays_read_their_items_not_the_header_lanes)
{
	auto data = Collections::from_json(R"({
		"version": 1, "collections": [{
			"class": "example", "key": "default", "parent": "", "source": "fixture",
			"sha256": "", "fields": { "thresholds": {
				"type": "EA::Reflection::Float", "data": "00030003",
				"array": {"capacity": 3, "element_size": 4, "alignment": 4,
				          "items": ["3FC00000", "40266666", "405CCCCD"]}
			}}
		}]
	})");
	CHECK_OK(data);
	auto items = data->float_items("example", "default", "thresholds");
	CHECK_OK(items);
	CHECK_EQ(items->size(), (size_t)3);
	CHECK_EQ((*items)[0], 1.5f);
	CHECK_EQ(f32_to_bits((*items)[2]), 0x405CCCCDu);
}

S3_TEST(collections_boolean_integer_words_and_type_errors)
{
	auto data = Collections::from_json(R"({"version":1,"collections":[{"class":"c","key":"k","parent":"","source":"","sha256":"",
		"fields":{"b":{"type":"EA::Reflection::Bool","data":"01000000"},"b0":{"type":"EA::Reflection::Bool","data":"00"},
		"i":{"type":"EA::Reflection::Int32","data":"FFFFFFFE"},"w":{"type":"EA::Reflection::UInt8","data":"0000 0001 0000 0002"}}}]})");
	CHECK_OK(data);
	CHECK(data->boolean("c", "k", "b").value == true);
	CHECK(data->boolean("c", "k", "b0").ok && data->boolean("c", "k", "b0").value == false);
	CHECK_EQ(data->integer("c", "k", "i").value, 0xFFFFFFFEu);
	auto w = data->words<2>("c", "k", "w");
	CHECK_OK(w);
	CHECK(w.value[0] == 1 && w.value[1] == 2);
	CHECK_ERR_CONTAINS(data->words<3>("c", "k", "w"), "Expected 3 big-endian words, found 16 bytes of hex");
	CHECK_ERR_CONTAINS(data->float_("c", "k", "b"), "Expected float at c/k/b");
	CHECK_ERR_CONTAINS(data->float_("c", "nope", "b"), "Missing stock collection c/nope");
}

S3_TEST(attrib_hash_numeric_names)
{
	// Hash_/0x spellings parse; anything else hashes. Hash of "" is 0.
	CHECK_EQ(attrib_hash::numeric_name("Hash_6918469984A8C596"), std::string("Hash_6918469984A8C596"));
	CHECK_EQ(attrib_hash::numeric_name("0x6918469984a8c596"), std::string("Hash_6918469984A8C596"));
	CHECK_EQ(attrib_hash::numeric_name(""), std::string("Hash_0000000000000000"));
	CHECK_EQ(attrib_hash::hash(""), (uint64_t)0);
	// The vault's numeric name for the readable `manual_clamp_vel` field (fixture above).
	CHECK_EQ(attrib_hash::numeric_name("manual_clamp_vel"), std::string("Hash_A353CE1670D3AA40"));
	// scoring.rs: `scoring_trick` resolves to the Scorable class identity.
	CHECK_EQ(attrib_hash::numeric_name("scoring_trick"), std::string("Hash_6918469984A8C596"));
}

S3_TEST(sha256_known_vectors)
{
	CHECK_EQ(sha256::digest((const uint8_t *)"", 0), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
	CHECK_EQ(sha256::digest((const uint8_t *)"abc", 3), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
	std::string m = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	CHECK_EQ(sha256::digest((const uint8_t *)m.data(), m.size()), std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
}

// scoring.rs: scoring_presence_accepts_readable_and_numeric_vault_names
S3_TEST(scoring_presence_accepts_readable_and_numeric_vault_names)
{
	std::string key = scoring_tables::IDENTIFIERS[0].name;
	std::string numeric_key = attrib_hash::numeric_name(key);
	for (const char *cls : {"scoring_trick", "Hash_6918469984A8C596", "0x6918469984a8c596"}) {
		for (const std::string &name : {key, numeric_key}) {
			auto data = Collections::from_json(std::string(R"({"version": 1, "collections": [{"class": ")") + cls + R"(", "key": ")" + name +
			                                   R"(", "parent": "", "source": "fixture", "sha256": "", "fields": {}},
				{"class": "unrelated", "key": "other", "parent": "", "source": "fixture", "sha256": "", "fields": {}}]})");
			CHECK_OK(data);
			auto keys = scorable_keys(data.value);
			CHECK(keys.size() == 1 && *keys.begin() == numeric_key);
		}
	}
}

// skate-core scoring/catalog.rs: the_metric_scorables_resolve_without_an_authored_record
// (identifier/class/score_type part; repetition rules are core).
S3_TEST(scoring_catalog_metric_scorables)
{
	const std::pair<size_t, const char *> metrics[] = {{129, "air_horizontal_distance"}, {130, "air_height_to_peak"},
	                                                   {131, "air_total_height_gain"},   {132, "air_player_spin"},
	                                                   {133, "air_player_flip"},         {237, "air_metric"},
	                                                   {253, "generic_metric"}};
	for (auto &m : metrics) {
		CHECK_EQ(std::string(scoring_tables::IDENTIFIERS[m.first].name), std::string(m.second));
		CHECK_EQ(scoring_tables::IDENTIFIERS[m.first].class_, 5u);
		CHECK_EQ(scoring_tables::IDENTIFIERS[m.first].score_type, (size_t)0);
	}
	CHECK_EQ(std::string(scoring_tables::IDENTIFIERS[0].name), std::string("fspowerslide"));
	CHECK_EQ(std::string(scoring_tables::IDENTIFIERS[331].name), std::string("lateflip_darkcatch"));
}

S3_TEST(point_graph_and_attribute_name_encoding)
{
	PointGraph8 g;
	for (int i = 0; i < 8; i++) {
		g.x[i] = (float)i;
		g.y[i] = (float)(i * 2);
	}
	CHECK_EQ(g.evaluate(-1.0f), 0.0f);
	CHECK_EQ(g.evaluate(2.5f), 5.0f);
	CHECK_EQ(g.evaluate(100.0f), 14.0f);
	// encode: digits 0-9 -> 1..10, A-Z -> 11..36, '_' -> 37, a-z like A-Z.
	AttributeName n = encode_attribute_name("a");
	CHECK_EQ(n[0], 11u * 79235168u);
	CHECK(encode_attribute_name("abc") == encode_attribute_name("ABC"));
}

S3_TEST(json_parser_strictness_and_numbers)
{
	CHECK(JsonDoc::parse(R"({"a":[1,2,3],"b":{"c":"\u00e9\ud83d\ude00"},"d":-0.5e3,"e":true,"f":null})").ok);
	CHECK(!JsonDoc::parse("{\"a\":1,}").ok);
	CHECK(!JsonDoc::parse("[1 2]").ok);
	CHECK(!JsonDoc::parse("01").ok);
	CHECK(!JsonDoc::parse("{\"a\":1} x").ok);
	CHECK(!JsonDoc::parse("\"\\ud800\"").ok);
	CHECK(!JsonDoc::parse("\"a\nb\"").ok);
	auto d = JsonDoc::parse(R"({"u":4294967295,"o":4294967296,"f":1.0,"n":-1,"s":"x\"y"})");
	CHECK_OK(d);
	JsonValue r = d->root();
	CHECK_EQ(r.get("u").to_u32().value, 4294967295u);
	CHECK(!r.get("o").to_u32().ok);
	CHECK(!r.get("f").to_u32().ok);
	CHECK(!r.get("n").to_u32().ok);
	CHECK_EQ(r.get("n").to_i32().value, -1);
	CHECK_EQ(r.get("s").to_string().value, std::string("x\"y"));
	// deny_unknown_fields / duplicate / missing field handling
	JsonFields strict(r, {"u", "o", "f", "n"}, true);
	CHECK(!strict.ok());
	JsonFields loose(r, {"u"}, false);
	CHECK(loose.ok());
	CHECK(!loose.require("zz").ok);
	auto dup = JsonDoc::parse(R"({"a":1,"a":2})");
	JsonFields dupf(dup->root(), {"a"}, false);
	CHECK(!dupf.ok());
}

S3_TEST(rust_f32_parse_grammar)
{
	float v;
	CHECK(parse_f32("0.55", v) && f32_to_bits(v) == f32_to_bits(0.55f));
	CHECK(parse_f32("-1", v) && v == -1.0f);
	CHECK(parse_f32("+.5", v) && v == 0.5f);
	CHECK(parse_f32("5.", v) && v == 5.0f);
	CHECK(parse_f32("1e3", v) && v == 1000.0f);
	CHECK(parse_f32("inf", v) && std::isinf(v));
	CHECK(parse_f32("NaN", v) && std::isnan(v));
	CHECK(!parse_f32("0x10", v));
	CHECK(!parse_f32("1e", v));
	CHECK(!parse_f32(".", v));
	CHECK(!parse_f32(" 1", v));
	CHECK(!parse_f32("1f", v));
	// Correct rounding of a halfway-ish decimal (Rust and strtof agree).
	CHECK(parse_f32("0.400000006", v) && f32_to_bits(v) == 0x3ecccccdu);
}
