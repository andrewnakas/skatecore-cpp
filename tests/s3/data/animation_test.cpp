// Ported tests: skate-core animation/vbr/tests/bits.rs (the VBR decoder ships with the data
// stage for animation_frames), plus ABIN reader / metadata unit checks.
#include "s3/data/abin.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/data/vbr.hpp"
#include "test.hpp"

using namespace s3::data;

// vbr/tests/bits.rs: native_sign_bit_and_zero_width_do_not_consume_extra_selectors
S3_TEST(vbr_native_sign_bit_and_zero_width_do_not_consume_extra_selectors)
{
	// Widths3,0,2: selector bits11, packed negative5=1010 and positive2=101.
	const uint8_t a[] = {2, 3, 0x5a};
	auto values = vbr::unpack(a, sizeof a, {0x0203});
	CHECK_OK(values);
	CHECK_EQ(values.value[0][0], -5.0f);
	CHECK_EQ(values.value[0][1], 0.0f);
	CHECK_EQ(values.value[0][2], 2.0f);
	const uint8_t b[] = {1, 1, 0};
	auto negative_zero = vbr::unpack(b, sizeof b, {1});
	CHECK_OK(negative_zero);
	CHECK_EQ(f32_to_bits(negative_zero.value[0][0]), 0x80000000u);
}

// vbr/tests/bits.rs: packed_words_continue_across_the_native_vector_boundary
S3_TEST(vbr_packed_words_continue_across_the_native_vector_boundary)
{
	// Nine width15 coefficients consume nine sign+magnitude16-bit words.
	std::vector<uint8_t> bytes = {9, 0xff, 1};
	for (uint8_t v : {0xff, 0xff, 2, 0, 5, 0, 6, 0, 9, 0, 10, 0, 13, 0, 14, 0, 17, 0}) bytes.push_back(v);
	auto values = vbr::unpack(bytes.data(), bytes.size(), {0xffffffffu, 15});
	CHECK_OK(values);
	std::array<float, 8> expected = {32767.0f, -1.0f, 2.0f, -3.0f, 4.0f, -5.0f, 6.0f, -7.0f};
	CHECK(values.value[0] == expected);
	CHECK_EQ(values.value[1][0], 8.0f);
	bytes.pop_back();
	CHECK(!vbr::unpack(bytes.data(), bytes.size(), {0xffffffffu, 15}).ok);
}

S3_TEST(vbr_cosine_matches_reference_points)
{
	// The TU3 cosine polynomial is close to libm on [-pi, pi].
	CHECK_EQ(vbr::trig_cos(0.0f), 1.0f);
	CHECK(std::abs(vbr::trig_cos(3.14159265f) + 1.0f) < 1e-5f);
	CHECK(std::abs(vbr::trig_cos(1.0f) - 0.5403023f) < 1e-6f);
}

S3_TEST(abin_reader_fast_string_names_and_bounds)
{
	// "AB" = (11*38 + 12) * 38^4, padding zero digits after.
	uint32_t word = (11u * 38u + 12u) * 38u * 38u * 38u * 38u;
	std::vector<uint8_t> bytes = {(uint8_t)(word >> 24), (uint8_t)(word >> 16), (uint8_t)(word >> 8), (uint8_t)word, 0, 0, 0, 0};
	abin::Reader r(bytes.data(), bytes.size());
	auto name = r.name(0, 2);
	CHECK_OK(name);
	CHECK_EQ(name.value, std::string("AB"));
	CHECK_ERR_CONTAINS(r.u32(6), "ABIN at 0x6: read size4 exceeds record 0x0..0x8");
	auto sub = r.bounded(abin::Range{0, 4});
	CHECK_OK(sub);
	CHECK(!sub->u32(4).ok);
	// Invalid padding: a zero digit followed by a nonzero remainder.
	uint32_t bad = 5;
	std::vector<uint8_t> pad = {0, 0, 0, (uint8_t)bad};
	abin::Reader r2(pad.data(), pad.size());
	CHECK_ERR_CONTAINS(r2.name(0, 1), "invalid FastString padding");
	CHECK_ERR_CONTAINS(abin::Bank::parse(std::vector<uint8_t>(8, 0)), "invalid type1 bank wrapper");
}

S3_TEST(animation_metadata_validates_names_and_json)
{
	CHECK(validate_name("R_IDLE_HCOM_000", 36).ok);
	CHECK_ERR_CONTAINS(validate_name("lower", 36), "Invalid canonical FastString name \"lower\"");
	CHECK(!validate_name("", 36).ok);
	std::string sha(64, 'a');
	std::string json = R"({"version":1,"source_bank":"OnBoard.abin","source_sha256":")" + sha +
	                   R"(","source_bytes":4096,"clips":[{"name":"CLIP_A","source_offset":64,"fps_bits":1106247680,"frames_bits":1065353216,"base_speed_bits":1065353216,"flags_word":0,"attributes":[]},
	                   {"name":"CLIP_A","source_offset":128,"fps_bits":1106247680,"frames_bits":1073741824,"base_speed_bits":1065353216,"flags_word":0,"attributes":[]}],
	                   "unsupported_trees":[{"name":"TREE_X","source_offset":256,"type_id":9}]})";
	auto m = AnimationMetadata::parse(json);
	CHECK_OK(m);
	auto clip = m->clip("clip_a");
	CHECK_OK(clip);
	CHECK_EQ(clip.value->source_offset, (uint64_t)128); // later bank record wins
	CHECK_EQ(m->clip_count(), (size_t)2);
	CHECK_ERR_CONTAINS(m->tree("TREE_X"), "is tree type 9; tree evaluation is required");
	auto tree = m->tree("CLIP_A");
	CHECK_OK(tree);
	CHECK(std::get<const ClipMetadata *>(tree.value)->source_offset == 128);
	CHECK_ERR_CONTAINS(AnimationMetadata::parse(R"({"version":2})"), "missing field");
	CHECK(m->source_for("clip_a") != nullptr);
}
