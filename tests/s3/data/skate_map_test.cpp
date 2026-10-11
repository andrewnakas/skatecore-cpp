// Ported tests: skate-data skate_map/{storage_v15,texture_decode}.rs, tests/skate_map.rs,
// tests/retail_collision.rs; plus zlib/zstd decoder vectors.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "compression_vectors.hpp"
#include "s3/data/attrib_hash.hpp"
#include "s3/data/inflate.hpp"
#include "s3/data/skate_map.hpp"
#include "s3/data/zstd.hpp"
#include "test.hpp"

using namespace s3::data;
using namespace s3::data::skate_map_storage;
using s3test::put_f32;
using s3test::put_u32;

namespace {

std::vector<uint8_t> le_words(std::initializer_list<uint32_t> words)
{
	std::vector<uint8_t> b;
	for (uint32_t w : words) put_u32(b, w);
	return b;
}

// storage_v15.rs tests::unpack (zstd for methods <= 6, zlib otherwise).
Result<std::vector<uint8_t>> unpack(uint32_t method, const std::vector<uint8_t> &data, size_t expected)
{
	std::vector<uint8_t> bytes;
	put_u32(bytes, (uint32_t)data.size());
	std::vector<uint8_t> packed = method <= 6 ? s3test::zstd_raw(data) : s3test::zlib_stored(data);
	bytes.insert(bytes.end(), packed.begin(), packed.end());
	return decode_v15(method, bytes.data(), bytes.size(), expected);
}

} // namespace

// storage_v15.rs: both_codecs_restore_vertices_indices_collision_and_rgba_exactly
S3_TEST(storage_v15_both_codecs_restore_vertices_indices_collision_and_rgba_exactly)
{
	std::vector<uint8_t> vertices(112);
	for (int i = 0; i < 112; i++) vertices[i] = (uint8_t)i;
	std::vector<uint8_t> soa;
	const size_t streams[7][2] = {{0, 12}, {12, 12}, {24, 8}, {32, 8}, {40, 4}, {44, 8}, {52, 4}};
	for (auto &s : streams)
		for (size_t v = 0; v < 2; v++) soa.insert(soa.end(), vertices.begin() + v * 56 + s[0], vertices.begin() + v * 56 + s[0] + s[1]);
	std::vector<uint8_t> collision;
	put_u32(collision, 3);
	for (int i = 0; i < 36; i++) collision.push_back((uint8_t)i);
	for (uint32_t index : {2u, 0u, 1u}) put_u32(collision, index);
	for (int i = 36; i < 48; i++) collision.push_back((uint8_t)i);
	std::vector<uint8_t> expected_collision;
	for (int i = 24; i < 36; i++) expected_collision.push_back((uint8_t)i);
	for (int i = 0; i < 24; i++) expected_collision.push_back((uint8_t)i);
	for (int i = 36; i < 48; i++) expected_collision.push_back((uint8_t)i);
	std::vector<uint8_t> rgba;
	put_u32(rgba, 2);
	put_u32(rgba, 5);
	for (uint8_t filter = 0; filter < 5; filter++) {
		rgba.push_back(filter);
		// Constant RGBA rows exercise all five PNG-style predictors.
		std::vector<uint8_t> row = filter == 0   ? std::vector<uint8_t>(8, 10)
		                           : filter == 1 ? std::vector<uint8_t>{10, 10, 10, 10, 0, 0, 0, 0}
		                           : filter == 3 ? std::vector<uint8_t>{5, 5, 5, 5, 0, 0, 0, 0}
		                                         : std::vector<uint8_t>(8, 0);
		rgba.insert(rgba.end(), row.begin(), row.end());
	}
	for (uint32_t shift : {0u, 4u}) {
		auto a = unpack(3 + shift, rgba, 40);
		CHECK_OK(a);
		CHECK(a.value == std::vector<uint8_t>(40, 10));
		auto b = unpack(4 + shift, soa, 112);
		CHECK_OK(b);
		CHECK(b.value == vertices);
		auto c = unpack(5 + shift, {0, 4, 1}, 12);
		CHECK_OK(c);
		CHECK(c.value == le_words({0, 2, 1}));
		auto d = unpack(6 + shift, collision, 48);
		CHECK_OK(d);
		CHECK(d.value == expected_collision);
	}
}

// storage_v15.rs: malformed_transforms_fail_without_panics
S3_TEST(storage_v15_malformed_transforms_fail_without_panics)
{
	for (uint32_t method = 3; method <= 10; method++) {
		CHECK(!unpack(method, {}, 48).ok);
		const uint8_t ff[4] = {255, 255, 255, 255};
		CHECK(!decode_v15(method, ff, 4, 48).ok);
	}
	CHECK(!unpack(9, {1}, 4).ok); // negative first index
	CHECK(!unpack(9, std::vector<uint8_t>(5, 128), 4).ok);
	CHECK(!unpack(9, {0, 0}, 4).ok); // trailing varint
	CHECK(!unpack(8, std::vector<uint8_t>(56, 0), 55).ok);
	std::vector<uint8_t> bad_collision;
	put_u32(bad_collision, 0);
	bad_collision.resize(bad_collision.size() + 24, 0);
	CHECK(!unpack(10, bad_collision, 48).ok);
}

// texture_decode.rs: mixed_storage_keeps_order_and_rejects_corruption_in_parallel
S3_TEST(texture_decode_mixed_storage_keeps_order_and_rejects_corruption)
{
	std::vector<std::vector<uint8_t>> raw, encoded;
	for (size_t i = 0; i < 16; i++) raw.push_back(std::vector<uint8_t>((i + 1) * 32 * 1024, (uint8_t)i));
	for (size_t i = 0; i < raw.size(); i++)
		encoded.push_back(i % 3 == 0 ? raw[i] : i % 3 == 1 ? s3test::zlib_stored(raw[i]) : s3test::zstd_raw(raw[i]));
	auto make_textures = [&]() {
		std::vector<Texture> t;
		for (size_t i = 0; i < raw.size(); i++) t.push_back(Texture{"texture-" + std::to_string(i), 1, 1, 0, {}});
		return t;
	};
	auto blocks_of = [&]() {
		std::vector<StoredBlock> b;
		for (size_t i = 0; i < encoded.size(); i++) b.push_back(StoredBlock{raw[i].size(), (uint32_t)(i % 3), encoded[i].data(), encoded[i].size()});
		return b;
	};
	{
		auto blocks = blocks_of();
		auto serial = make_textures();
		CHECK_OK(decode_textures(serial, blocks, 1));
		for (size_t workers : {(size_t)2, (size_t)4, std::numeric_limits<size_t>::max()}) {
			auto parallel = make_textures();
			auto n = decode_textures(parallel, blocks, workers);
			CHECK_OK(n);
			CHECK_EQ(n.value, std::min<size_t>(workers, 8));
			for (size_t i = 0; i < raw.size(); i++) {
				CHECK(parallel[i].rgba == serial[i].rgba);
				CHECK(parallel[i].rgba == raw[i]);
			}
		}
	}
	encoded[5].resize(3);
	auto textures = make_textures();
	CHECK(!decode_textures(textures, blocks_of(), 4).ok);
}

// texture_decode.rs: stored_block_retains_size_and_deflate_tail_validation
S3_TEST(texture_decode_stored_block_retains_size_and_deflate_tail_validation)
{
	std::vector<uint8_t> bytes = s3test::zlib_stored(std::vector<uint8_t>(64, 7));
	CHECK(!(StoredBlock{63, 1, bytes.data(), bytes.size()}.decode().ok));
	bytes.push_back(42);
	CHECK(!(StoredBlock{64, 1, bytes.data(), bytes.size()}.decode().ok));
	CHECK(!(StoredBlock{1, 0, nullptr, 0}.decode().ok));
	CHECK(!(StoredBlock{0, 99, nullptr, 0}.decode().ok));
}

S3_TEST(compression_vectors_zlib_and_zstd_decode_exactly)
{
	for (const auto &v : s3test::vectors::ALL) {
		auto r = StoredBlock{v.size, (uint32_t)v.method, v.packed, v.packed_size}.decode();
		CHECK_MSG(r.ok, std::string(v.name) + ": " + r.error);
		CHECK_MSG(r.value.size() == v.size, v.name);
		CHECK_MSG(sha256::digest(r.value.data(), r.value.size()) == v.sha256, v.name);
		// Every truncation fails (no silent partial output of the expected size).
		for (size_t cut : {v.packed_size / 2, v.packed_size - 1}) {
			auto t = StoredBlock{v.size, (uint32_t)v.method, v.packed, cut}.decode();
			CHECK_MSG(!t.ok, std::string(v.name) + " accepted truncation " + std::to_string(cut));
		}
	}
	// Corrupt checksum is rejected by zstd; corrupt adler by zlib.
	const auto &z = s3test::vectors::ALL[0];
	std::vector<uint8_t> bad(z.packed, z.packed + z.packed_size);
	bad.back() ^= 1;
	CHECK(!(StoredBlock{z.size, 1, bad.data(), bad.size()}.decode().ok));
	for (const auto &v : s3test::vectors::ALL) {
		if (std::string(v.name) != "s1_l3") continue;
		std::vector<uint8_t> b(v.packed, v.packed + v.packed_size);
		b.back() ^= 0x40;
		CHECK_ERR_CONTAINS((StoredBlock{v.size, 2, b.data(), b.size()}.decode()), "checksum");
	}
	CHECK_EQ(zstd::xxh64(nullptr, 0, 0), 0xEF46DB3751D8E999ull);
	CHECK_EQ(zstd::xxh64((const uint8_t *)"abc", 3, 0), 0x44BC2CF5AD770999ull);
}

// ---- tests/skate_map.rs ----

namespace {

void s_(std::vector<uint8_t> &b, const std::string &value)
{
	put_u32(b, (uint32_t)value.size());
	b.insert(b.end(), value.begin(), value.end());
}
void fs(std::vector<uint8_t> &b, std::initializer_list<float> values)
{
	for (float v : values) put_f32(b, v);
}
void fs_n(std::vector<uint8_t> &b, size_t n)
{
	for (size_t i = 0; i < n; i++) put_f32(b, 0.0f);
}
void stored(std::vector<uint8_t> &b, const std::vector<uint8_t> &data, uint8_t method)
{
	std::vector<uint8_t> bytes = method == 0 ? data : method == 1 ? s3test::zlib_stored(data) : s3test::zstd_raw(data);
	put_u32(b, method);
	put_u32(b, (uint32_t)bytes.size());
	b.insert(b.end(), bytes.begin(), bytes.end());
}

std::vector<uint8_t> fixture(uint8_t version)
{
	char magic[9];
	std::snprintf(magic, sizeof magic, "SKATE%02u", version);
	std::vector<uint8_t> b(magic, magic + 7);
	b.push_back(0);
	put_u32(b, 0x12345678);
	s_(b, "Fixture");
	fs(b, {2., 0., 3., 1.});
	fs_n(b, 9);
	fs(b, {0., 12., 0.});
	if (version >= 3) fs(b, {17., 0.});
	if (version >= 6) fs_n(b, 31);
	for (uint32_t n : {1, 1, 3, 3, 1, 1}) put_u32(b, n);
	if (version >= 4) put_u32(b, 0);
	if (version >= 7) put_u32(b, 1);
	if (version >= 8) put_u32(b, 1);
	size_t material_start = b.size();
	s_(b, "Surface");
	put_u32(b, 1);
	fs(b, {0.6f, 0.1f, 1., 0.5, 0.25, 0.8f, 0.});
	put_u32(b, 1);
	put_u32(b, 0);
	put_f32(b, 1.);
	if (version >= 2) {
		for (uint32_t n : {0, 0, 0, 1}) put_u32(b, n);
		put_f32(b, 0.4f);
		for (uint32_t n : {42, 4, 7}) put_u32(b, n);
	}
	if (version >= 13) put_u32(b, 1);
	if (version >= 12) put_u32(b, 0);
	if (version >= 15) {
		std::vector<uint8_t> material(b.begin() + material_start, b.end());
		b.resize(material_start);
		put_u32(b, (uint32_t)material.size());
		stored(b, material, 2);
	}
	s_(b, "Pixel");
	for (uint32_t n : {1, 1, 1}) put_u32(b, n);
	if (version >= 9) {
		stored(b, {255, 128, 64, 255}, version % 3);
	} else {
		put_u32(b, 4);
		for (uint8_t v : {255, 128, 64, 255}) b.push_back(v);
	}
	const float points[3][3] = {{0., 0., 0.}, {0., 0., 1.}, {1., 0., 0.}};
	std::vector<uint8_t> vertices;
	for (auto &p : points) {
		fs(vertices, {p[0], p[1], p[2]});
		fs(vertices, {0., 1., 0., 0., 0., 0., 0.});
		put_u32(vertices, 1);
		if (version >= 12) {
			fs(vertices, {0., 0.});
			for (uint8_t v : {0, 0, 127, 127}) vertices.push_back(v);
		}
	}
	std::vector<uint8_t> indices;
	for (uint32_t n : {0, 1, 2}) put_u32(indices, n);
	std::vector<uint8_t> collision;
	for (auto &p : points) fs(collision, {p[0], p[1], p[2]});
	put_u32(collision, 99);
	put_u32(collision, 1);
	if (version >= 11)
		for (uint8_t v : {26, 26, 26, 0}) collision.push_back(v);
	for (auto *bytes : {&vertices, &indices, &collision}) {
		if (version >= 9) stored(b, *bytes, version % 3);
		else b.insert(b.end(), bytes->begin(), bytes->end());
	}
	s_(b, "Rail");
	put_u32(b, 0);
	if (version >= 10) put_u32(b, 0);
	put_u32(b, 2);
	fs(b, {0., 1., 0., 2., 1., 0.});
	if (version >= 7) {
		s_(b, "Spot");
		put_u32(b, 1);
		fs(b, {0., 2., 0., 0., -1., 0., 1., 0.8f, 0.5, 100., 10., 0.1f, 0.95f, 0.8f});
	}
	if (version >= 8) {
		s_(b, "Route");
		put_u32(b, 1);
		put_u32(b, 2);
		fs(b, {4., 3.});
		put_u32(b, 2);
		fs(b, {0., 0., 0., 3., 0., 0.});
	}
	if (version >= 12) {
		put_u32(b, 1);
		for (char c : std::string("WMET")) b.push_back((uint8_t)c);
		put_u32(b, 1);
		put_u32(b, 2);
		stored(b, {'{', '}'}, version % 3);
	}
	return b;
}

size_t find(const std::vector<uint8_t> &data, const std::vector<uint8_t> &needle)
{
	auto it = std::search(data.begin(), data.end(), needle.begin(), needle.end());
	return it == data.end() ? std::string::npos : (size_t)(it - data.begin());
}

Result<SkateMap> parse(const std::vector<uint8_t> &d) { return SkateMap::parse(d.data(), d.size()); }

} // namespace

// tests/skate_map.rs: presentation_loading_does_not_relax_playable_map_validation
S3_TEST(skate_map_presentation_loading_does_not_relax_playable_map_validation)
{
	auto data = fixture(1);
	size_t counts = 8 + 4 + 4 + 7 + (4 + 12) * 4;
	std::fill(data.begin() + counts + 16, data.begin() + counts + 24, 0); // No collision or rails.
	size_t indices = find(data, le_words({0, 1, 2}));
	CHECK(indices != std::string::npos);
	data.resize(indices + 12);
	CHECK_ERR_CONTAINS(parse(data), "requires triangle collision");
	auto render = SkateMap::parse_render_only(data.data(), data.size());
	CHECK_OK(render);
	CHECK(render->geometry.indices == std::vector<uint32_t>({0, 1, 2}));
	CHECK(render->geometry.collision.empty());
	std::vector<uint8_t> n99 = le_words({99});
	std::copy(n99.begin(), n99.end(), data.begin() + indices);
	CHECK_ERR_CONTAINS(SkateMap::parse_render_only(data.data(), data.size()), "indices");
	auto full = fixture(1);
	CHECK_ERR_CONTAINS(SkateMap::parse_render_only(full.data(), full.size()), "non-presentation");
}

// tests/skate_map.rs: reads_all_documented_versions_without_moving_geometry
S3_TEST(skate_map_reads_all_documented_versions_without_moving_geometry)
{
	for (uint8_t version = 1; version <= 15; version++) {
		auto map = parse(fixture(version));
		CHECK_MSG(map.ok, "version " + std::to_string(version) + ": " + map.error);
		CHECK_EQ(map->version, version);
		CHECK(map->spawn == (Vec3f{2., 0., 3.}));
		CHECK_EQ(map->heading, 1.0f);
		CHECK(map->geometry.vertices[2].position == (Vec3f{1., 0., 0.}));
		CHECK_EQ(map->geometry.collision[0].surface, 99u);
		CHECK(map->textures[0].rgba == std::vector<uint8_t>({255, 128, 64, 255}));
		CHECK_EQ(map->materials[0].audio, version == 1 ? 3u : 42u);
		CHECK_EQ(map->materials[0].physics, version == 1 ? 1u : 4u);
		CHECK_EQ(map->rails[0].points.size(), (size_t)2);
		CHECK_EQ(map->lights.size(), (size_t)(version >= 7));
		CHECK_EQ(map->routes.size(), (size_t)(version >= 8));
		if (version >= 12) {
			CHECK(map->extensions[0].payload == std::vector<uint8_t>({'{', '}'}));
			CHECK(map->geometry.vertices[0].tangent_frame == (std::optional<std::array<uint8_t, 4>>(std::array<uint8_t, 4>{0, 0, 127, 127})));
		}
	}
}

// tests/skate_map.rs: v15_texture_references_preserve_order_and_reject_forward_or_wrong_size
S3_TEST(skate_map_v15_texture_references_preserve_order_and_reject_forward_or_wrong_size)
{
	auto data = fixture(15);
	size_t header_counts = 8 + 4 + 4 + 7 + 49 * 4;
	std::vector<uint8_t> three = le_words({3});
	std::copy(three.begin(), three.end(), data.begin() + header_counts + 4);
	std::vector<uint8_t> needle;
	s_(needle, "Pixel");
	size_t start = find(data, needle);
	CHECK(start != std::string::npos);
	size_t metadata_end = start + needle.size() + 12;
	size_t texture_end = metadata_end + 8 + 4; // fixture v15 uses raw RGBA
	std::vector<uint8_t> references;
	for (uint32_t source : {0u, 1u}) {
		references.insert(references.end(), data.begin() + start, data.begin() + metadata_end);
		put_u32(references, 11);
		put_u32(references, 4);
		put_u32(references, source);
	}
	data.insert(data.begin() + texture_end, references.begin(), references.end());
	auto map = parse(data);
	CHECK_OK(map);
	CHECK_EQ(map->textures.size(), (size_t)3);
	for (auto &t : map->textures) CHECK(t.rgba == std::vector<uint8_t>({255, 128, 64, 255}));
	size_t reference = texture_end + (metadata_end - start) + 8;
	std::vector<uint8_t> one = le_words({1}), zero = le_words({0}), two = le_words({2});
	std::copy(one.begin(), one.end(), data.begin() + reference);
	CHECK_ERR_CONTAINS(parse(data), "forward texture reference");
	std::copy(zero.begin(), zero.end(), data.begin() + reference);
	size_t width = texture_end + needle.size();
	std::copy(two.begin(), two.end(), data.begin() + width);
	CHECK_ERR_CONTAINS(parse(data), "reference size mismatch");
}

// tests/skate_map.rs: rejects_every_truncated_prefix_and_trailing_data
S3_TEST(skate_map_rejects_every_truncated_prefix_and_trailing_data)
{
	auto data = fixture(8);
	for (size_t end = 0; end < data.size(); end++)
		CHECK_MSG(!SkateMap::parse(data.data(), end).ok, "accepted truncation at " + std::to_string(end));
	data.push_back(0);
	CHECK(!parse(data).ok);
}

// tests/skate_map.rs: rejects_future_versions_endianness_nonfinite_and_huge_counts
S3_TEST(skate_map_rejects_future_versions_endianness_nonfinite_and_huge_counts)
{
	auto data = fixture(8);
	auto bad = data;
	bad[5] = '9';
	bad[6] = '9';
	CHECK_ERR_CONTAINS(parse(bad), "version");
	bad = data;
	bad[8] = 0;
	CHECK_ERR_CONTAINS(parse(bad), "endian");
	bad = data;
	float nan = std::numeric_limits<float>::quiet_NaN();
	std::memcpy(&bad[23], &nan, 4);
	CHECK_ERR_CONTAINS(parse(bad), "Non-finite");
	bad = data;
	std::vector<uint8_t> max = le_words({0xFFFFFFFFu});
	std::copy(max.begin(), max.end(), bad.begin() + 219);
	CHECK(!parse(bad).ok);
}

// tests/skate_map.rs: rejects_bad_vertex_and_material_references
S3_TEST(skate_map_rejects_bad_vertex_and_material_references)
{
	auto data = fixture(1);
	// Locate the known three-index sequence immediately before collision data.
	size_t offset = find(data, le_words({0, 1, 2}));
	CHECK(offset != std::string::npos);
	auto bad = data;
	std::vector<uint8_t> n99 = le_words({99}), zero = le_words({0});
	std::copy(n99.begin(), n99.end(), bad.begin() + offset);
	CHECK_ERR_CONTAINS(parse(bad), "indices");
	bad = data;
	std::copy(zero.begin(), zero.end(), bad.begin() + offset - 4);
	CHECK_ERR_CONTAINS(parse(bad), "reference");
}

// ---- tests/retail_collision.rs ----
// The Rust fixture `tests/fixtures/retail-collision.rwcmset` is not in the reference checkout;
// this rebuilds an archive with exactly the properties the tests describe: three meshes
// `compression-0..2`, one cluster each at mesh+160 using vertex compression modes 0/1/2
// (granularity 0.25), one one-sided triangle [[x,0,0],[x,0,1],[x+1,0,0]] (x = 10*mesh) with
// edges 20 42 9a, little-endian group 0x1234 and surface 0x4321.

namespace {

void be32(std::vector<uint8_t> &b, size_t at, uint32_t v)
{
	for (int i = 0; i < 4; i++) b[at + i] = (uint8_t)(v >> (24 - 8 * i));
}
void be16(std::vector<uint8_t> &b, size_t at, uint16_t v)
{
	b[at] = (uint8_t)(v >> 8);
	b[at + 1] = (uint8_t)v;
}

std::vector<uint8_t> rwcm_mesh(int mode, float x)
{
	const float granularity = 0.25f;
	float pts[3][3] = {{x, 0, 0}, {x, 0, 1}, {x + 1, 0, 0}};
	// Cluster
	size_t vertex_end = mode == 0 ? 16 + 3 * 16 : mode == 1 ? 28 + 3 * 6 : 16 + 3 * 12;
	size_t unit_start = (vertex_end + 15) / 16 * 16;
	std::vector<uint8_t> unit = {0xE1, 0, 1, 2, 0x20, 0x42, 0x9a, 0x34, 0x12, 0x21, 0x43};
	std::vector<uint8_t> cluster(unit_start + unit.size(), 0);
	be16(cluster, 0, 1);
	be16(cluster, 2, (uint16_t)unit.size());
	be16(cluster, 4, (uint16_t)(unit_start / 16 - 1));
	be16(cluster, 8, (uint16_t)cluster.size());
	cluster[10] = 3;
	cluster[12] = (uint8_t)mode;
	for (int i = 0; i < 3; i++) {
		for (int axis = 0; axis < 3; axis++) {
			if (mode == 0) {
				uint32_t bits;
				std::memcpy(&bits, &pts[i][axis], 4);
				be32(cluster, 16 + i * 16 + axis * 4, bits);
			} else if (mode == 1) {
				// Base = vertex 0; deltas are unsigned offsets from it.
				int32_t base = (int32_t)(pts[0][axis] / granularity);
				if (i == 0) be32(cluster, 16 + axis * 4, (uint32_t)base);
				be16(cluster, 28 + i * 6 + axis * 2, (uint16_t)((int32_t)(pts[i][axis] / granularity) - base));
			} else {
				be32(cluster, 16 + i * 12 + axis * 4, (uint32_t)(int32_t)(pts[i][axis] / granularity));
			}
		}
	}
	std::copy(unit.begin(), unit.end(), cluster.begin() + unit_start);
	// Mesh: 96-byte header, cluster table at 96, cluster at 160.
	std::vector<uint8_t> mesh(160 + cluster.size(), 0);
	be32(mesh, 40, 1);                 // triangle count
	be32(mesh, 52, 96);                // cluster table
	uint32_t gbits;
	std::memcpy(&gbits, &granularity, 4);
	be32(mesh, 56, gbits);             // granularity
	be16(mesh, 60, 0x10);              // one-sided flag
	mesh[62] = 2;                      // group id width
	mesh[63] = 2;                      // surface id width
	be32(mesh, 64, 1);                 // cluster count
	be32(mesh, 80, (uint32_t)mesh.size());
	be32(mesh, 96, 160);
	std::copy(cluster.begin(), cluster.end(), mesh.begin() + 160);
	return mesh;
}

std::vector<uint8_t> rwcm_fixture()
{
	std::vector<uint8_t> b = {'R', 'W', 'C', 'M', 'S', 'E', 'T', '1'};
	put_u32(b, 3);
	for (int i = 0; i < 3; i++) {
		std::string name = "compression-" + std::to_string(i);
		put_u32(b, (uint32_t)name.size());
		b.insert(b.end(), name.begin(), name.end());
		auto mesh = rwcm_mesh(i, (float)i * 10.0f);
		put_u32(b, (uint32_t)mesh.size());
		b.insert(b.end(), mesh.begin(), mesh.end());
	}
	return b;
}

} // namespace

// tests/retail_collision.rs: compressed_offsets_zero_extend_across_the_sign_boundary
S3_TEST(retail_collision_compressed_offsets_zero_extend_across_the_sign_boundary)
{
	auto bytes = rwcm_fixture();
	// Locate the second fixture mesh, which uses 16-bit offsets.
	size_t at = 12, mesh = 0;
	for (int k = 0; k < 2; k++) {
		uint32_t name_len;
		std::memcpy(&name_len, &bytes[at], 4);
		at += 4 + name_len;
		uint32_t size;
		std::memcpy(&size, &bytes[at], 4);
		mesh = at + 4;
		at = mesh + size;
	}
	size_t cluster = mesh + 160;
	const int32_t base[3] = {-40000, -32768, INT32_MAX - 2};
	for (int axis = 0; axis < 3; axis++) be32(bytes, cluster + 16 + axis * 4, (uint32_t)base[axis]);
	const uint16_t raws[3] = {0x7fff, 0x8000, 0xffff};
	for (int i = 0; i < 3; i++)
		for (int axis = 0; axis < 3; axis++) be16(bytes, cluster + 28 + i * 6 + axis * 2, raws[i]);
	bool seen = false;
	std::string failure;
	auto r = retail_collision::visit_clusters(bytes.data(), bytes.size(), [&](const std::string &name, const std::vector<RetailTriangle> &t) -> Status {
		if (name == "compression-1") {
			seen = true;
			std::array<Vec3f, 3> expected = {Vec3f{-1808.25f, -0.25f, 536870912.f}, Vec3f{-1808.f, 0.f, 536870912.f},
			                                 Vec3f{6383.75f, 8191.75f, 536870912.f}};
			if (t[0].points != expected) failure = "points differ";
		}
		return Unit{};
	});
	CHECK_OK(r);
	CHECK(seen);
	CHECK_MSG(failure.empty(), failure);
}

// tests/retail_collision.rs: preserves_all_vertex_encodings_and_little_endian_unit_ids
S3_TEST(retail_collision_preserves_all_vertex_encodings_and_little_endian_unit_ids)
{
	auto fixture_bytes = rwcm_fixture();
	size_t clusters = 0;
	std::string failure;
	auto total = retail_collision::visit_clusters(fixture_bytes.data(), fixture_bytes.size(),
	                                              [&](const std::string &name, const std::vector<RetailTriangle> &triangles) -> Status {
		                                              if (name != "compression-" + std::to_string(clusters)) failure = "name " + name;
		                                              if (triangles.size() != 1) failure = "count";
		                                              const RetailTriangle &t = triangles[0];
		                                              float x = (float)clusters * 10.f;
		                                              std::array<Vec3f, 3> expected = {Vec3f{x, 0, 0}, Vec3f{x, 0, 1}, Vec3f{x + 1, 0, 0}};
		                                              if (t.points != expected) failure = "points " + name;
		                                              if (!t.edges || *t.edges != (std::array<uint8_t, 3>{0x20, 0x42, 0x9a})) failure = "edges";
		                                              if (t.surface != 0x4321 || t.group != 0x1234 || !t.one_sided) failure = "ids";
		                                              clusters++;
		                                              return Unit{};
	                                              });
	CHECK_OK(total);
	CHECK_MSG(failure.empty(), failure);
	CHECK_EQ(total.value, (size_t)3);
	CHECK_EQ(clusters, (size_t)3);
}

// tests/retail_collision.rs: rejects_every_truncated_prefix_and_trailing_bytes
S3_TEST(retail_collision_rejects_every_truncated_prefix_and_trailing_bytes)
{
	auto fixture_bytes = rwcm_fixture();
	auto none = [](const std::string &, const std::vector<RetailTriangle> &) -> Status { return Unit{}; };
	for (size_t end = 0; end < fixture_bytes.size(); end++)
		CHECK_MSG(!retail_collision::visit_clusters(fixture_bytes.data(), end, none).ok, "prefix " + std::to_string(end));
	fixture_bytes.push_back(0);
	CHECK(!retail_collision::visit_clusters(fixture_bytes.data(), fixture_bytes.size(), none).ok);
}

// spawn_ground: a map with ground below keeps its spawn; one without moves above the nearest walkable centre.
S3_TEST(spawn_ground_finds_ground_or_nearest_walkable)
{
	auto map = parse(fixture(8));
	CHECK_OK(map);
	// The fixture's one collision triangle lies in y=0 over x,z in [0,1].
	auto above = spawn_ground::grounded_spawn(map.value, Vec3f{0.2f, 3.0f, 0.2f});
	CHECK_OK(above);
	CHECK(!above->has_value());
	auto off = spawn_ground::grounded_spawn(map.value, Vec3f{50.f, 3.0f, 50.f});
	CHECK_OK(off);
	CHECK(off->has_value());
	Vec3f c = **off;
	CHECK_EQ(c[0], 1.0f / 3.0f);
	CHECK_EQ(c[1], 1.0f);
	CHECK_EQ(c[2], 1.0f / 3.0f);
}
