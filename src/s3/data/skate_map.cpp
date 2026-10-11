// SKATE01..15 reader (skate_map.rs, storage_v15.rs, texture_decode.rs).
#include "skate_map.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <set>

#include "inflate.hpp"
#include "zstd.hpp"

namespace s3::data {

namespace {

struct Reader
{
	const uint8_t *bytes = nullptr;
	size_t size = 0;
	size_t at = 0;

	Result<const uint8_t *> take(size_t n)
	{
		if (n > std::numeric_limits<size_t>::max() - at) return Error{"SKATE offset overflow"};
		size_t end = at + n;
		if (end > size) return Error{"Truncated SKATE at byte " + std::to_string(at) + " (need " + std::to_string(n) + ")"};
		const uint8_t *p = bytes + at;
		at = end;
		return p;
	}
	Result<uint32_t> u()
	{
		S3_TRY(const uint8_t *p, take(4));
		return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
	}
	Result<float> f()
	{
		S3_TRY(uint32_t bits, u());
		float value = f32_from_bits(bits);
		if (!is_finite(value)) return Error{"Non-finite SKATE float at " + std::to_string(at - 4)};
		return value;
	}
	template <size_t N>
	Result<std::array<float, N>> floats()
	{
		std::array<float, N> out{};
		for (float &v : out) {
			S3_TRY(v, f());
		}
		return out;
	}
	Result<std::string> string()
	{
		S3_TRY(uint32_t n, u());
		S3_TRY(const uint8_t *p, take(n));
		if (!is_utf8(p, n)) return Error{"Invalid SKATE UTF-8"};
		return std::string((const char *)p, n);
	}
	Status check_count(uint32_t count, size_t minimum_bytes) const
	{
		if ((size_t)count > (size - at) / minimum_bytes) return Error{"SKATE count exceeds remaining data"};
		return Unit{};
	}
	Result<std::vector<Vec3f>> points()
	{
		S3_TRY(uint32_t n, u());
		S3_CHECK(check_count(n, 12));
		std::vector<Vec3f> out;
		out.reserve(n);
		for (uint32_t i = 0; i < n; i++) {
			S3_TRY(Vec3f p, floats<3>());
			out.push_back(p);
		}
		return out;
	}
	Result<skate_map_storage::StoredBlock> stored_block(size_t expected)
	{
		if (expected > 2147483648ull) return Error{"SKATE decoded block exceeds 2 GiB reader limit"};
		S3_TRY(uint32_t method, u());
		S3_TRY(uint32_t n, u());
		S3_TRY(const uint8_t *p, take(n));
		return skate_map_storage::StoredBlock{expected, method, p, n};
	}
	Result<std::vector<uint8_t>> stored(size_t expected)
	{
		S3_TRY(skate_map_storage::StoredBlock block, stored_block(expected));
		return block.decode();
	}
	Result<Geometry> geometry(std::array<uint32_t, 3> counts, size_t materials, uint8_t version);
};

Result<Geometry> Reader::geometry(std::array<uint32_t, 3> counts, size_t materials, uint8_t version)
{
	uint32_t nv = counts[0], ni = counts[1], nc = counts[2];
	S3_CHECK(check_count(nv, version >= 12 ? 56 : 44));
	Geometry g;
	g.vertices.reserve(nv);
	for (uint32_t i = 0; i < nv; i++) {
		Vertex v;
		S3_TRY(v.position, floats<3>());
		S3_TRY(v.normal, floats<3>());
		S3_TRY(v.uv, floats<2>());
		S3_TRY(v.lightmap_uv, floats<2>());
		S3_TRY(v.material, u());
		if (version >= 12) {
			S3_TRY(auto decal, floats<2>());
			v.decal_uv = decal;
			S3_TRY(const uint8_t *t, take(4));
			v.tangent_frame = std::array<uint8_t, 4>{t[0], t[1], t[2], t[3]};
		}
		g.vertices.push_back(v);
	}
	S3_CHECK(check_count(ni, 4));
	g.indices.reserve(ni);
	for (uint32_t i = 0; i < ni; i++) {
		S3_TRY(uint32_t index, u());
		g.indices.push_back(index);
	}
	S3_CHECK(check_count(nc, version >= 11 ? 48 : 44));
	g.collision.reserve(nc);
	for (uint32_t i = 0; i < nc; i++) {
		Collision c;
		for (auto &p : c.points) {
			S3_TRY(p, floats<3>());
		}
		S3_TRY(c.surface, u());
		S3_TRY(c.material, u());
		if (version >= 11) {
			S3_TRY(const uint8_t *e, take(4));
			if (e[3] != 0) c.native_edges = std::array<uint8_t, 3>{e[0], e[1], e[2]};
		}
		g.collision.push_back(c);
	}
	auto valid_material = [&](uint32_t id) { return id > 0 && (size_t)id <= materials; };
	bool bad = false;
	for (const Vertex &v : g.vertices) bad |= !valid_material(v.material);
	for (const Collision &c : g.collision) bad |= !valid_material(c.material) || c.surface == 0;
	if (bad) return Error{"SKATE invalid material/surface reference"};
	bad = g.indices.size() % 3 != 0;
	for (uint32_t i : g.indices) bad |= (size_t)i >= g.vertices.size();
	if (bad) return Error{"SKATE invalid triangle indices"};
	for (size_t t = 0; t + 2 < g.indices.size(); t += 3) {
		uint32_t m = g.vertices[g.indices[t]].material;
		for (int k = 0; k < 3; k++)
			if (g.vertices[g.indices[t + k]].material != m) return Error{"SKATE triangle mixes materials"};
	}
	for (const Collision &tri : g.collision) {
		const Vec3f &a = tri.points[0], &b = tri.points[1], &c = tri.points[2];
		float u0 = b[0] - a[0], u1 = b[1] - a[1], u2 = b[2] - a[2];
		float v0 = c[0] - a[0], v1 = c[1] - a[1], v2 = c[2] - a[2];
		float cross[3] = {u1 * v2 - u2 * v1, u2 * v0 - u0 * v2, u0 * v1 - u1 * v0};
		// Iterator::sum::<f32> starts from -0.0 (Rust >= 1.82) and adds in order.
		float area = -0.0f;
		for (float x : cross) area = area + x * x;
		if (!is_finite(area) || area <= 0.0f) return Error{"SKATE degenerate collision triangle"};
	}
	return g;
}

} // namespace

// ---- StoredBlock / storage_v15 ----

namespace skate_map_storage {

Result<std::vector<uint8_t>> StoredBlock::decode() const
{
	std::vector<uint8_t> decoded;
	switch (method) {
	case 0: decoded.assign(bytes, bytes + size); break;
	case 1: {
		inflate::Output out = inflate::zlib_decompress(bytes, size, expected + 1);
		if (!out.error.empty()) return Error{"SKATE DEFLATE: " + out.error};
		if (out.total_in != size) return Error{"SKATE compressed block has unused input"};
		decoded = std::move(out.bytes);
		break;
	}
	case 2: {
		zstd::Output out = zstd::decompress(bytes, size, expected + 1);
		if (!out.error.empty()) return Error{"SKATE Zstandard: " + out.error};
		decoded = std::move(out.bytes);
		break;
	}
	case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10: return decode_v15(method, bytes, size, expected);
	default: return Error{"Unsupported SKATE storage method " + std::to_string(method)};
	}
	if (decoded.size() != expected) return Error{"SKATE decoded block size mismatch"};
	return decoded;
}

Result<std::vector<uint8_t>> decode_v15(uint32_t method, const uint8_t *bytes, size_t size, size_t expected)
{
	Reader input{bytes, size, 0};
	S3_TRY(uint32_t size32, input.u());
	size_t transformed = size32;
	// Transformed records are at most a small multiple of their decoded size.
	// Bound both lengths before allocating or decompressing untrusted data.
	size_t bound = expected > (std::numeric_limits<size_t>::max() - 16) / 3 ? std::numeric_limits<size_t>::max() : expected * 3 + 16;
	if (transformed > bound || transformed > 2147483648ull) return Error{"SKATE transformed block exceeds size limit"};
	StoredBlock inner{transformed, method <= 6 ? 2u : 1u, bytes + 4, size - 4};
	S3_TRY(std::vector<uint8_t> data, inner.decode());
	Reader source{data.data(), data.size(), 0};
	std::vector<uint8_t> result;
	switch (method) {
	case 3:
	case 7: {
		S3_TRY(uint32_t width32, source.u());
		S3_TRY(uint32_t height32, source.u());
		size_t width = width32, height = height32;
		if (width > std::numeric_limits<size_t>::max() / 4) return Error{"SKATE filtered width overflow"};
		size_t row = width * 4;
		bool product_ok = height == 0 || row <= std::numeric_limits<size_t>::max() / height;
		if (!product_ok || row * height != expected || width > 16384 || height > 16384) return Error{"SKATE filtered dimensions mismatch"};
		result.assign(expected, 0);
		for (size_t y = 0; y < height; y++) {
			S3_TRY(const uint8_t *fp, source.take(1));
			uint8_t filter = fp[0];
			if (filter > 4) return Error{"Invalid SKATE RGBA filter"};
			S3_TRY(const uint8_t *values, source.take(row));
			for (size_t x = 0; x < row; x++) {
				size_t at = y * row + x;
				int left = x >= 4 ? result[at - 4] : 0;
				int above = y > 0 ? result[at - row] : 0;
				int upper_left = (x >= 4 && y > 0) ? result[at - row - 4] : 0;
				int predictor;
				switch (filter) {
				case 1: predictor = left; break;
				case 2: predictor = above; break;
				case 3: predictor = (left + above) / 2; break;
				case 4: {
					int p = left + above - upper_left;
					int a = p - left < 0 ? left - p : p - left;
					int b = p - above < 0 ? above - p : p - above;
					int c = p - upper_left < 0 ? upper_left - p : p - upper_left;
					predictor = (a <= b && a <= c) ? left : (b <= c) ? above : upper_left;
					break;
				}
				default: predictor = 0;
				}
				result[at] = (uint8_t)(values[x] + (uint8_t)predictor);
			}
		}
		break;
	}
	case 4:
	case 8: {
		if (expected % 56 != 0 || data.size() != expected) return Error{"Invalid SKATE vertex streams"};
		result.assign(expected, 0);
		static const size_t streams[7][2] = {{0, 12}, {12, 12}, {24, 8}, {32, 8}, {40, 4}, {44, 8}, {52, 4}};
		for (auto &s : streams) {
			for (size_t vertex = 0; vertex < expected / 56; vertex++) {
				size_t at = vertex * 56 + s[0];
				S3_TRY(const uint8_t *p, source.take(s[1]));
				std::memcpy(result.data() + at, p, s[1]);
			}
		}
		break;
	}
	case 5:
	case 9: {
		if (expected % 4 != 0) return Error{"Invalid SKATE index block size"};
		int64_t previous = 0;
		result.reserve(expected);
		for (size_t k = 0; k < expected / 4; k++) {
			uint64_t zigzag = 0;
			// A delta between u32 indices requires at most 33 bits.
			for (int shift = 0; shift <= 28; shift += 7) {
				S3_TRY(const uint8_t *bp, source.take(1));
				uint8_t byte = bp[0];
				if (shift == 28 && byte > 31) return Error{"Invalid SKATE index varint"};
				zigzag |= (uint64_t)(byte & 127) << shift;
				if ((byte & 128) == 0) break;
			}
			int64_t current = previous + ((int64_t)(zigzag >> 1) ^ -(int64_t)(zigzag & 1));
			if (current < 0 || current > (int64_t)0xffffffffll) return Error{"SKATE index delta overflow"};
			uint32_t c = (uint32_t)current;
			for (int b = 0; b < 4; b++) result.push_back((uint8_t)(c >> (8 * b)));
			previous = (int64_t)c;
		}
		break;
	}
	case 6:
	case 10: {
		if (expected % 48 != 0) return Error{"Invalid SKATE collision block size"};
		S3_TRY(uint32_t count32, source.u());
		size_t count = count32;
		if (count > std::numeric_limits<size_t>::max() / 12) return Error{"SKATE collision vertex overflow"};
		S3_TRY(const uint8_t *vertices, source.take(count * 12));
		result.reserve(expected);
		for (size_t k = 0; k < expected / 48; k++) {
			for (int j = 0; j < 3; j++) {
				S3_TRY(uint32_t index32, source.u());
				size_t index = index32;
				if (index >= count) return Error{"Invalid SKATE collision vertex reference"};
				result.insert(result.end(), vertices + index * 12, vertices + index * 12 + 12);
			}
			S3_TRY(const uint8_t *tail, source.take(12));
			result.insert(result.end(), tail, tail + 12);
		}
		break;
	}
	default: return Error{"Invalid SKATE storage transform"};
	}
	if (source.at != data.size() || result.size() != expected) return Error{"SKATE transformed size mismatch or trailing data"};
	return result;
}

Result<size_t> decode_textures(std::vector<Texture> &textures, const std::vector<StoredBlock> &blocks, size_t workers)
{
	size_t bytes = 0;
	for (const StoredBlock &b : blocks) bytes = b.expected > std::numeric_limits<size_t>::max() - bytes ? std::numeric_limits<size_t>::max() : bytes + b.expected;
	size_t count = 1;
	if (bytes >= 1024 * 1024) {
		count = workers < 1 ? 1 : workers > 8 ? 8 : workers;
		size_t cap = blocks.empty() ? 1 : blocks.size();
		if (count > cap) count = cap;
	}
	// Serial decode in index order; errors carry the texture index when the Rust
	// decoder would have run in parallel.
	for (size_t i = 0; i < textures.size() && i < blocks.size(); i++) {
		Result<std::vector<uint8_t>> rgba = blocks[i].decode();
		if (!rgba.ok) return Error{count == 1 ? rgba.error : "Texture " + std::to_string(i) + ": " + rgba.error};
		textures[i].rgba = std::move(rgba.value);
	}
	return count;
}

} // namespace skate_map_storage

using skate_map_storage::StoredBlock;

Status validate_static_objects(const SkateMap &map, const Extension &extension)
{
	if (extension.schema != 3) return Error{"Unsupported MOBJ schema"};
	Reader r{extension.payload.data(), extension.payload.size(), 0};
	S3_TRY(uint32_t count, r.u());
	S3_CHECK(r.check_count(count, 80));
	std::set<uint32_t> ids;
	for (uint32_t k = 0; k < count; k++) {
		S3_TRY(uint32_t id, r.u());
		if (!ids.insert(id).second) return Error{"Duplicate MOBJ identity"};
		S3_TRY(std::string name, r.string());
		S3_CHECK(r.floats<3>());
		for (size_t limit : {map.geometry.indices.size(), map.geometry.collision.size()}) {
			S3_TRY(uint32_t first, r.u());
			S3_TRY(uint32_t length, r.u());
			if ((size_t)first + (size_t)length > limit) return Error{"MOBJ " + name + " geometry range is invalid"};
		}
		S3_TRY(uint32_t rails, r.u());
		S3_CHECK(r.check_count(rails, 4));
		for (uint32_t i = 0; i < rails; i++) {
			S3_TRY(uint32_t rail, r.u());
			if ((size_t)rail >= map.rails.size()) return Error{"MOBJ " + name + " rail index is invalid"};
		}
		S3_TRY(uint32_t physics, r.u());
		if (physics != 0) return Error{"MOBJ " + name + " requests object physics, which requires a body adapter"};
		S3_TRY(uint32_t shape, r.u());
		if (shape > 2) return Error{"MOBJ " + name + " collision shape is invalid"};
		S3_CHECK(r.floats<6>());
		for (int i = 0; i < 2; i++) {
			S3_TRY(uint32_t flag, r.u());
			if (flag > 1) return Error{"MOBJ " + name + " has an invalid boolean"};
		}
	}
	if (r.at != r.size) return Error{"MOBJ has trailing data"};
	return Unit{};
}

static Result<SkateMap> parse_inner(const uint8_t *data, size_t data_size, size_t workers, bool require_collision)
{
	Reader r{data, data_size, 0};
	S3_TRY(const uint8_t *magic, r.take(8));
	auto digit = [](uint8_t c) { return c >= '0' && c <= '9'; };
	if (std::memcmp(magic, "SKATE", 5) != 0 || magic[7] != 0 || !digit(magic[5]) || !digit(magic[6]))
		return Error{"Invalid SKATE magic/version"};
	uint8_t version = (uint8_t)((magic[5] - '0') * 10 + magic[6] - '0');
	if (version < 1 || version > 15)
		return Error{"Unsupported SKATE version: reader supports 01 through 15; newer packages require an update"};
	S3_TRY(uint32_t marker, r.u());
	if (marker != 0x12345678u) return Error{"Invalid SKATE endian marker"};
	SkateMap map;
	map.version = version;
	S3_TRY(map.name, r.string());
	if (map.name.empty()) return Error{"Empty SKATE map name"};
	S3_TRY(map.spawn, r.floats<3>());
	S3_TRY(map.heading, r.f());
	size_t ne = version >= 6 ? 45 : version >= 3 ? 14 : 12;
	for (size_t i = 0; i < ne; i++) {
		S3_TRY(float v, r.f());
		map.environment.push_back(v);
	}
	uint32_t counts[9] = {0};
	size_t n = version >= 8 ? 9 : version >= 7 ? 8 : version >= 4 ? 7 : 6;
	for (size_t i = 0; i < n; i++) {
		S3_TRY(counts[i], r.u());
	}
	std::vector<uint8_t> material_bytes;
	bool have_material_bytes = false;
	if (version >= 15) {
		S3_TRY(uint32_t size, r.u());
		S3_TRY(material_bytes, r.stored(size));
		have_material_bytes = true;
	}
	Reader package_reader;
	bool have_package = false;
	if (have_material_bytes) {
		package_reader = r;
		have_package = true;
		r = Reader{material_bytes.data(), material_bytes.size(), 0};
	}
	S3_CHECK(r.check_count(counts[0], version >= 2 ? 80 : 48));
	for (uint32_t k = 0; k < counts[0]; k++) {
		Material m;
		S3_TRY(m.name, r.string());
		S3_TRY(m.flags, r.u());
		S3_TRY(m.friction, r.f());
		S3_TRY(m.restitution, r.f());
		S3_TRY(m.color, r.floats<3>());
		S3_TRY(m.roughness, r.f());
		S3_TRY(m.emissive, r.f());
		S3_TRY(m.textures[0], r.u());
		S3_TRY(m.textures[1], r.u());
		S3_TRY(m.indirect_strength, r.f());
		if (version >= 2) {
			for (int i = 2; i < 5; i++) {
				S3_TRY(m.textures[i], r.u());
			}
			S3_TRY(m.alpha_mode, r.u());
			S3_TRY(m.alpha_cutoff, r.f());
			S3_TRY(m.audio, r.u());
			S3_TRY(m.physics, r.u());
			S3_TRY(m.pattern, r.u());
		}
		if (version >= 13) {
			S3_TRY(uint32_t layer, r.u());
			if (layer > 3) return Error{"Invalid SKATE depth layer"};
			m.depth_layer = layer;
		}
		if (version >= 12) {
			S3_TRY(uint32_t has_retail, r.u());
			if (has_retail != 0) {
				size_t start = r.at;
				S3_CHECK(r.take(16));
				S3_CHECK(r.string());
				S3_CHECK(r.take(8));
				S3_TRY(uint32_t bindings, r.u());
				S3_CHECK(r.check_count(bindings, 20));
				for (uint32_t b = 0; b < bindings; b++) {
					S3_CHECK(r.string());
					S3_TRY(uint32_t id, r.u());
					S3_TRY(uint32_t uv, r.u());
					S3_TRY(uint32_t au, r.u());
					S3_TRY(uint32_t av, r.u());
					if (id > counts[1] || uv > 2 || au > 1 || av > 1) return Error{"Invalid SKATE retail texture binding"};
				}
				S3_TRY(uint32_t parameters, r.u());
				S3_CHECK(r.check_count(parameters, 8));
				for (uint32_t p = 0; p < parameters; p++) {
					S3_CHECK(r.string());
					S3_TRY(uint32_t nn, r.u());
					S3_CHECK(r.check_count(nn, 4));
					for (uint32_t q = 0; q < nn; q++) S3_CHECK(r.string());
				}
				S3_CHECK(r.string());
				m.retail_definition = std::vector<uint8_t>(r.bytes + start, r.bytes + r.at);
			}
		}
		bool bad = m.name.empty() || m.alpha_mode > 2 || m.audio > 127 || m.physics > 13 || m.pattern > 15 || m.friction < 0.0f ||
		           m.restitution < 0.0f || !(m.roughness >= 0.0f && m.roughness <= 1.0f) || !(m.alpha_cutoff >= 0.0f && m.alpha_cutoff <= 1.0f) ||
		           m.indirect_strength < 0.0f;
		for (uint32_t id : m.textures) bad |= id > counts[1];
		if (bad) return Error{"Invalid SKATE material"};
		map.materials.push_back(std::move(m));
	}
	if (have_package) {
		if (r.at != r.size) return Error{"SKATE material block has trailing bytes"};
		r = package_reader;
	}
	S3_CHECK(r.check_count(counts[1], 20));
	std::vector<StoredBlock> texture_blocks;
	for (uint32_t k = 0; k < counts[1]; k++) {
		S3_TRY(std::string name, r.string());
		S3_TRY(uint32_t width, r.u());
		S3_TRY(uint32_t height, r.u());
		S3_TRY(uint32_t color_space, r.u());
		if ((width == 0) != (height == 0) || width > 16384 || height > 16384 || color_space > 1) return Error{"Invalid SKATE embedded texture"};
		size_t expected = (size_t)width * (size_t)height * 4;
		StoredBlock block;
		if (version >= 9) {
			S3_TRY(block, r.stored_block(expected));
		} else {
			S3_TRY(uint32_t bytes, r.u());
			if ((size_t)bytes != expected) return Error{"Invalid SKATE embedded texture size"};
			S3_TRY(const uint8_t *p, r.take(bytes));
			block = StoredBlock{expected, 0, p, bytes};
		}
		if (version >= 15 && block.method == 11) {
			if (block.size != 4) return Error{"Invalid SKATE texture reference size"};
			size_t index = (size_t)block.bytes[0] | (size_t)block.bytes[1] << 8 | (size_t)block.bytes[2] << 16 | (size_t)block.bytes[3] << 24;
			if (index >= texture_blocks.size()) return Error{"Invalid SKATE forward texture reference"};
			StoredBlock source = texture_blocks[index];
			if (source.expected != expected) return Error{"SKATE texture reference size mismatch"};
			block = source;
		}
		texture_blocks.push_back(block);
		map.textures.push_back(Texture{std::move(name), width, height, color_space, {}});
	}
	S3_CHECK(skate_map_storage::decode_textures(map.textures, texture_blocks, workers));
	std::array<uint32_t, 3> geometry_counts{counts[2], counts[3], counts[4]};
	if (version >= 9) {
		S3_TRY(std::vector<uint8_t> bytes, r.stored((size_t)counts[2] * (version >= 12 ? 56 : 44)));
		S3_TRY(std::vector<uint8_t> indices, r.stored((size_t)counts[3] * 4));
		bytes.insert(bytes.end(), indices.begin(), indices.end());
		S3_TRY(std::vector<uint8_t> collision, r.stored((size_t)counts[4] * (version >= 11 ? 48 : 44)));
		bytes.insert(bytes.end(), collision.begin(), collision.end());
		Reader g{bytes.data(), bytes.size(), 0};
		S3_TRY(map.geometry, g.geometry(geometry_counts, map.materials.size(), version));
	} else {
		S3_TRY(map.geometry, r.geometry(geometry_counts, map.materials.size(), version));
	}
	if (map.materials.empty() || map.geometry.vertices.empty() || map.geometry.indices.empty())
		return Error{"SKATE requires materials and render geometry"};
	S3_CHECK(r.check_count(counts[5], 12));
	for (uint32_t k = 0; k < counts[5]; k++) {
		Rail rail;
		S3_TRY(rail.name, r.string());
		S3_TRY(uint32_t closed, r.u());
		rail.closed = closed != 0;
		uint32_t representation = 0;
		if (version >= 10) {
			S3_TRY(representation, r.u());
		}
		if (representation == 0) {
			S3_TRY(rail.points, r.points());
		} else if (representation == 1) {
			size_t start = r.at;
			S3_CHECK(r.take(24));
			S3_TRY(uint32_t nn, r.u());
			S3_CHECK(r.check_count(nn, 120));
			S3_CHECK(r.take((size_t)nn * 120));
			rail.native = std::vector<uint8_t>(r.bytes + start, r.bytes + r.at);
		} else {
			return Error{"Unsupported SKATE rail representation"};
		}
		map.rails.push_back(std::move(rail));
	}
	S3_CHECK(r.check_count(counts[6], 120));
	for (uint32_t k = 0; k < counts[6]; k++) {
		Door door;
		S3_TRY(door.name, r.string());
		for (auto &f : door.frame) {
			S3_TRY(f, r.floats<3>());
		}
		S3_TRY(door.motion, r.floats<5>());
		if (version >= 5) {
			S3_TRY(Vec3f response, r.floats<3>());
			door.response = response;
		}
		S3_TRY(door.friction, r.f());
		S3_TRY(door.restitution, r.f());
		S3_TRY(door.surface, r.u());
		std::array<uint32_t, 3> dc{};
		for (auto &c : dc) {
			S3_TRY(c, r.u());
		}
		S3_TRY(door.geometry, r.geometry(dc, map.materials.size(), version));
		map.doors.push_back(std::move(door));
	}
	S3_CHECK(r.check_count(counts[7], 64));
	for (uint32_t k = 0; k < counts[7]; k++) {
		Light light;
		S3_TRY(light.name, r.string());
		S3_TRY(light.kind, r.u());
		S3_TRY(light.position, r.floats<3>());
		S3_TRY(light.direction, r.floats<3>());
		S3_TRY(light.color, r.floats<3>());
		S3_TRY(light.intensity, r.f());
		S3_TRY(light.range, r.f());
		S3_TRY(light.radius, r.f());
		S3_TRY(light.inner_cos, r.f());
		S3_TRY(light.outer_cos, r.f());
		if (light.kind > 2 || light.range <= 0.0f || light.intensity < 0.0f || light.radius < 0.0f ||
		    !(light.inner_cos >= -1.0f && light.inner_cos <= 1.0f) || !(light.outer_cos >= -1.0f && light.outer_cos <= 1.0f))
			return Error{"Invalid SKATE light"};
		map.lights.push_back(std::move(light));
	}
	S3_CHECK(r.check_count(counts[8], 24));
	for (uint32_t k = 0; k < counts[8]; k++) {
		Route route;
		S3_TRY(route.rail.name, r.string());
		S3_TRY(uint32_t closed, r.u());
		route.rail.closed = closed != 0;
		S3_TRY(route.skaters, r.u());
		S3_TRY(route.speed, r.f());
		S3_TRY(route.spacing, r.f());
		S3_TRY(route.rail.points, r.points());
		map.routes.push_back(std::move(route));
	}
	if (version >= 12) {
		S3_TRY(uint32_t count, r.u());
		S3_CHECK(r.check_count(count, 20));
		for (uint32_t k = 0; k < count; k++) {
			Extension e;
			S3_TRY(const uint8_t *tag, r.take(4));
			std::memcpy(e.tag.data(), tag, 4);
			S3_TRY(e.schema, r.u());
			S3_TRY(uint32_t bytes, r.u());
			S3_TRY(e.payload, r.stored(bytes));
			map.extensions.push_back(std::move(e));
		}
	}
	if (require_collision && map.geometry.collision.empty()) {
		bool archive = false;
		for (const Extension &e : map.extensions) archive |= e.tag_is("RWCM") && e.schema == 1 && !e.payload.empty();
		if (!archive) return Error{"SKATE requires triangle collision or an embedded RWCM archive"};
	}
	if (r.at != data_size) return Error{"SKATE contains " + std::to_string(data_size - r.at) + " unexplained trailing bytes"};
	return map;
}

Result<SkateMap> SkateMap::load(const Vfs &vfs, const std::string &path)
{
	S3_TRY_MAP(SharedBytes data, vfs.read(path), [&](const std::string &e) { return path + ": " + e; });
	S3_TRY_MAP(SkateMap map, parse(data.data, data.size), [&](const std::string &e) { return path + ": " + e; });
	return map;
}

Result<SkateMap> SkateMap::parse(const uint8_t *data, size_t size) { return parse_with_decode_workers(data, size, 1); }

Result<SkateMap> SkateMap::parse_with_decode_workers(const uint8_t *data, size_t size, size_t workers)
{
	return parse_inner(data, size, workers, true);
}

Result<SkateMap> SkateMap::parse_render_only(const uint8_t *data, size_t size)
{
	S3_TRY(SkateMap map, parse_inner(data, size, 4, false));
	bool other = false;
	for (const Extension &e : map.extensions) other |= !e.tag_is("WMET");
	if (!map.geometry.collision.empty() || !map.rails.empty() || !map.doors.empty() || !map.lights.empty() || !map.routes.empty() || other)
		return Error{"SKATE render-only package contains non-presentation data"};
	return map;
}

// ---- retail_collision.rs ----

namespace retail_collision {

namespace {
Result<const uint8_t *> span(const uint8_t *data, size_t size, size_t at, size_t n)
{
	if (n > std::numeric_limits<size_t>::max() - at) return Error{"RWCM offset overflow"};
	if (at + n > size) return Error{"Truncated RWCM at " + std::to_string(at) + " (need " + std::to_string(n) + ")"};
	return data + at;
}
Result<uint16_t> be16(const uint8_t *data, size_t size, size_t at)
{
	S3_TRY(const uint8_t *p, span(data, size, at, 2));
	return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}
Result<uint32_t> be32(const uint8_t *data, size_t size, size_t at)
{
	S3_TRY(const uint8_t *p, span(data, size, at, 4));
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
Result<size_t> le32(const uint8_t *data, size_t size, size_t &at)
{
	S3_TRY(const uint8_t *p, span(data, size, at, 4));
	at += 4;
	return (size_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}
Result<uint16_t> id(const uint8_t *data, size_t size, size_t &at, uint8_t width)
{
	uint16_t value;
	if (width == 1) {
		S3_TRY(const uint8_t *p, span(data, size, at, 1));
		value = p[0];
	} else if (width == 2) {
		S3_TRY(const uint8_t *p, span(data, size, at, 2));
		value = (uint16_t)(p[0] | (uint16_t)p[1] << 8);
	} else {
		return Error{"Unsupported RWCM ID width " + std::to_string(width)};
	}
	at += width;
	return value;
}

Result<std::vector<RetailTriangle>> decode_cluster(const uint8_t *data, size_t size, float granularity, uint8_t group_width,
                                                   uint8_t surface_width, bool one_sided)
{
	S3_TRY(uint16_t unit_count16, be16(data, size, 0));
	size_t unit_count = unit_count16;
	S3_TRY(uint16_t unit_block, be16(data, size, 4));
	size_t unit_start = ((size_t)unit_block + 1) * 16;
	S3_TRY(uint16_t unit_len, be16(data, size, 2));
	S3_TRY(const uint8_t *units, span(data, size, unit_start, unit_len));
	size_t units_size = unit_len;
	S3_TRY(const uint8_t *vertex_data, span(data, size, 0, unit_start));
	size_t vertex_size = unit_start;
	std::vector<Vec3f> vertices;
	vertices.reserve(data[10]);
	for (size_t i = 0; i < (size_t)data[10]; i++) {
		Vec3f p{};
		for (size_t axis = 0; axis < 3; axis++) {
			switch (data[12]) {
			case 0: {
				S3_TRY(uint32_t bits, be32(vertex_data, vertex_size, 16 + i * 16 + axis * 4));
				p[axis] = f32_from_bits(bits);
				break;
			}
			case 1: {
				S3_TRY(uint32_t base_bits, be32(vertex_data, vertex_size, 16 + axis * 4));
				int32_t base = (int32_t)base_bits;
				// vmrghh with a zero vector produces unsigned halfwords;
				// vaddsws then adds these to the signed base with saturation.
				S3_TRY(uint16_t delta, be16(vertex_data, vertex_size, 28 + i * 6 + axis * 2));
				int64_t sum = (int64_t)base + (int64_t)delta;
				int32_t integer = sum > INT32_MAX ? INT32_MAX : (int32_t)sum;
				// Float conversion precedes multiplication in GetVertex.
				p[axis] = (float)integer * granularity;
				break;
			}
			case 2: {
				S3_TRY(uint32_t bits, be32(vertex_data, vertex_size, 16 + i * 12 + axis * 4));
				p[axis] = (float)(int32_t)bits * granularity;
				break;
			}
			default: return Error{"Unsupported RWCM vertex compression " + std::to_string(data[12])};
			}
			if (!is_finite(p[axis])) return Error{"Non-finite RWCM vertex"};
		}
		vertices.push_back(p);
	}
	size_t at = 0;
	std::vector<RetailTriangle> triangles;
	for (size_t k = 0; k < unit_count; k++) {
		S3_TRY(const uint8_t *fp, span(units, units_size, at, 1));
		uint8_t flags = fp[0];
		at += 1;
		if ((flags & 15) != 1)
			return Error{"Unsupported RWCM unit type " + std::to_string(flags & 15) + "; triangle units are required"};
		S3_TRY(const uint8_t *indices, span(units, units_size, at, 3));
		at += 3;
		RetailTriangle t;
		for (int i = 0; i < 3; i++) {
			if ((size_t)indices[i] >= vertices.size()) return Error{"RWCM missing vertex"};
			t.points[i] = vertices[indices[i]];
		}
		if (flags & 0x20) {
			S3_TRY(const uint8_t *e, span(units, units_size, at, 3));
			at += 3;
			t.edges = std::array<uint8_t, 3>{e[0], e[1], e[2]};
		}
		if (flags & 0x40) {
			S3_TRY(t.group, id(units, units_size, at, group_width));
		}
		if (flags & 0x80) {
			S3_TRY(t.surface, id(units, units_size, at, surface_width));
		}
		t.one_sided = one_sided;
		triangles.push_back(t);
	}
	if (at != units_size) return Error{"RWCM unit stream has trailing bytes"};
	return triangles;
}

Result<size_t> mesh_clusters(const uint8_t *data, size_t full_size, const std::function<Status(const std::vector<RetailTriangle> &)> &visit)
{
	S3_CHECK(span(data, full_size, 0, 96));
	S3_TRY(uint32_t size32, be32(data, full_size, 80));
	size_t size = size32;
	if (size < 96 || size > full_size) return Error{"Invalid RWCM mesh size"};
	S3_TRY(uint32_t table32, be32(data, size, 52));
	size_t table = table32;
	S3_TRY(uint32_t count32, be32(data, size, 64));
	size_t count = count32;
	S3_TRY(uint32_t gbits, be32(data, size, 56));
	float granularity = f32_from_bits(gbits);
	if (!is_finite(granularity) || granularity <= 0.0f) return Error{"Invalid RWCM granularity"};
	if (table < 96 || count == 0 || count > size / 4) return Error{"Invalid RWCM cluster table"};
	S3_CHECK(span(data, size, table, count * 4));
	S3_TRY(uint16_t flags, be16(data, size, 60));
	bool one_sided = (flags & 0x10) != 0;
	size_t previous_end = table + count * 4;
	size_t total = 0;
	for (size_t i = 0; i < count; i++) {
		S3_TRY(uint32_t off32, be32(data, size, table + i * 4));
		size_t offset = off32;
		if (offset < previous_end) return Error{"Overlapping RWCM clusters"};
		S3_TRY(uint16_t csize16, be16(data, size, offset + 8));
		size_t csize = csize16;
		if (csize < 16) return Error{"Invalid RWCM cluster size"};
		S3_TRY(const uint8_t *cluster, span(data, size, offset, csize));
		previous_end = offset + csize;
		S3_TRY(std::vector<RetailTriangle> triangles, decode_cluster(cluster, csize, granularity, data[62], data[63], one_sided));
		total += triangles.size();
		S3_CHECK(visit(triangles));
	}
	S3_TRY(uint32_t expected, be32(data, size, 40));
	if (total != (size_t)expected) return Error{"RWCM triangle count mismatch"};
	return total;
}
} // namespace

Result<size_t> visit_clusters(const uint8_t *data, size_t size,
                              const std::function<Status(const std::string &, const std::vector<RetailTriangle> &)> &visit)
{
	S3_TRY(const uint8_t *magic, span(data, size, 0, 8));
	if (std::memcmp(magic, "RWCMSET1", 8) != 0) return Error{"Invalid RWCMSET1 magic"};
	size_t at = 8;
	S3_TRY(size_t count, le32(data, size, at));
	if (count == 0 || count > size / 105) return Error{"Invalid RWCM mesh count"};
	size_t total = 0;
	for (size_t k = 0; k < count; k++) {
		S3_TRY(size_t name_size, le32(data, size, at));
		if (name_size == 0 || name_size > 4096) return Error{"Invalid RWCM mesh name length"};
		S3_TRY(const uint8_t *name_p, span(data, size, at, name_size));
		if (!is_utf8(name_p, name_size)) return Error{"Invalid RWCM mesh name"};
		std::string name((const char *)name_p, name_size);
		at += name_size;
		S3_TRY(size_t mesh_size, le32(data, size, at));
		S3_TRY(const uint8_t *mesh, span(data, size, at, mesh_size));
		at += mesh_size;
		S3_TRY_MAP(size_t n, mesh_clusters(mesh, mesh_size, [&](const std::vector<RetailTriangle> &t) { return visit(name, t); }),
		           [&](const std::string &e) { return "RWCM mesh " + debug_str(name) + ": " + e; });
		total += n;
	}
	if (at != size) return Error{"RWCM archive has trailing bytes"};
	return total;
}

} // namespace retail_collision

// ---- spawn_ground.rs ----

namespace spawn_ground {

using Tri = std::array<Vec3f, 3>;

static Result<std::vector<Tri>> triangles(const SkateMap &map)
{
	std::vector<Tri> out;
	for (const Collision &c : map.geometry.collision) out.push_back(c.points);
	for (const Extension &e : map.extensions) {
		if (!e.tag_is("RWCM")) continue;
		S3_CHECK(retail_collision::visit_clusters(e.payload.data(), e.payload.size(),
		                                          [&](const std::string &, const std::vector<RetailTriangle> &cluster) -> Status {
			                                          for (const RetailTriangle &t : cluster) out.push_back(t.points);
			                                          return Unit{};
		                                          }));
		break; // `find`: the first RWCM extension only
	}
	return out;
}

/// Height of `t` at (x, z) if the vertical line through it crosses the triangle.
static std::optional<float> height_at(const Tri &t, float x, float z)
{
	const Vec3f &a = t[0], &b = t[1], &c = t[2];
	float d = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2]);
	if (std::fabs(d) < 1e-9f) return std::nullopt;
	float u = ((b[2] - c[2]) * (x - c[0]) + (c[0] - b[0]) * (z - c[2])) / d;
	float v = ((c[2] - a[2]) * (x - c[0]) + (a[0] - c[0]) * (z - c[2])) / d;
	float w = 1.0f - u - v;
	if (u >= -1e-4f && v >= -1e-4f && w >= -1e-4f) return u * a[1] + v * b[1] + w * c[1];
	return std::nullopt;
}

/// Upward-facing enough to stand on (normal within ~45 degrees of +Y).
static bool walkable(const Tri &t)
{
	const Vec3f &a = t[0], &b = t[1], &c = t[2];
	float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
	float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
	float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
	float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
	return len > 1e-9f && std::fabs(n[1] / len) > 0.7f;
}

// f32::total_cmp ordering key.
static int32_t total_key(float v)
{
	int32_t bits = (int32_t)f32_to_bits(v);
	return bits ^ (int32_t)((uint32_t)(bits >> 31) >> 1);
}

Result<std::optional<Vec3f>> grounded_spawn(const SkateMap &map, Vec3f point)
{
	S3_TRY(std::vector<Tri> tris, triangles(map));
	bool below = false;
	for (const Tri &t : tris) {
		if (!walkable(t)) continue;
		std::optional<float> y = height_at(t, point[0], point[2]);
		if (y && *y <= point[1] + 0.5f && *y >= point[1] - 200.0f) {
			below = true;
			break;
		}
	}
	if (below || tris.empty()) return std::optional<Vec3f>();
	auto centre = [](const Tri &t) {
		Vec3f c;
		for (int i = 0; i < 3; i++) c[i] = (t[0][i] + t[1][i] + t[2][i]) / 3.0f;
		return c;
	};
	auto dist = [&](const Vec3f &c) {
		float dx = c[0] - point[0], dz = c[2] - point[2], dy = c[1] - point[1];
		// powi(2) is a single multiply.
		return dx * dx + dz * dz + 0.25f * (dy * dy);
	};
	std::optional<Vec3f> best;
	float best_d = 0;
	for (const Tri &t : tris) {
		if (!walkable(t)) continue;
		Vec3f c = centre(t);
		float d = dist(c);
		// min_by keeps the first minimum.
		if (!best || total_key(d) < total_key(best_d)) {
			best = c;
			best_d = d;
		}
	}
	if (!best) return std::optional<Vec3f>();
	return std::optional<Vec3f>(Vec3f{(*best)[0], (*best)[1] + 1.0f, (*best)[2]});
}

} // namespace spawn_ground

} // namespace s3::data
