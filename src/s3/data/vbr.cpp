// Direct stock VBR SQT decoding (skate-core animation/vbr/*.rs).
#include "vbr.hpp"

#include <cmath>
#include <limits>

namespace s3::data::vbr {

// ---- header.rs helpers ----

static Result<size_t> offset(size_t base, size_t relative)
{
	if (relative > std::numeric_limits<size_t>::max() - base) return Error{"VBR offset overflow"};
	return base + relative;
}
static Result<const uint8_t *> take(const uint8_t *bytes, size_t size, size_t at, size_t count)
{
	S3_TRY(size_t end, offset(at, count));
	if (at > size || end > size) return Error{"VBR read " + hex_prefixed(at) + "+" + std::to_string(count) + " out of bounds"};
	return bytes + at;
}
static Result<uint16_t> u16_at(const uint8_t *bytes, size_t size, size_t at)
{
	S3_TRY(const uint8_t *p, take(bytes, size, at, 2));
	return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}
static Result<uint32_t> u32_at(const uint8_t *bytes, size_t size, size_t at)
{
	S3_TRY(const uint8_t *p, take(bytes, size, at, 4));
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

Result<Header> Header::read(const uint8_t *bytes, size_t size, size_t part, size_t header_relative, size_t data_relative)
{
	S3_TRY(uint32_t header_word, u32_at(bytes, size, part));
	size_t bones = (size_t)((header_word >> 6) & 63) + 1;
	S3_TRY(size_t header, offset(part, header_relative));
	S3_TRY(size_t header_end, offset(header, (size_t)(header_word >> 12)));
	S3_TRY(size_t data, offset(part, data_relative));
	S3_TRY(size_t size_at, offset(part, 4));
	S3_TRY(uint32_t data_size, u32_at(bytes, size, size_at));
	S3_TRY(size_t data_end, offset(data, (size_t)data_size));
	if (header_end < header) return Error{"VBR header range"};
	S3_CHECK(take(bytes, size, header, header_end - header));
	if (data_end < data) return Error{"VBR data range"};
	S3_CHECK(take(bytes, size, data, data_end - data));
	S3_TRY(uint16_t frames16, u16_at(bytes, size, header + 2));
	size_t frames = frames16;
	S3_TRY(const uint8_t *count, take(bytes, size, header + 12, 2));
	if (count[0] != 3 || frames == 0) return Error{"VBR SQT requires three channel types and a nonempty frame range"};
	size_t dictionary = header + 16;
	size_t dictionary_count = count[1];
	S3_TRY(size_t size_table, offset(dictionary, dictionary_count * 4));
	size_t block_count = (frames + 7) / 8;
	S3_TRY(size_t after_sizes, offset(size_table, block_count * 2));
	S3_TRY(size_t types_unaligned, offset(after_sizes, 15));
	size_t types = types_unaligned & ~(size_t)15;
	S3_TRY(size_t types_end, offset(types, 48));
	if (types_end > header_end) return Error{"VBR channel descriptions exceed compression header"};
	Header h;
	h.frames = frames;
	h.bones = bones;
	static const size_t expected[3] = {3, 4, 3};
	for (size_t i = 0; i < 3; i++) {
		size_t at = types + 16 * i;
		S3_TRY(uint16_t dynamic, u16_at(bytes, size, at));
		S3_TRY(uint16_t constant, u16_at(bytes, size, at + 2));
		S3_TRY(const uint8_t *components, take(bytes, size, at + 12, 1));
		S3_TRY(uint32_t min_bits, u32_at(bytes, size, at + 4));
		S3_TRY(uint32_t max_bits, u32_at(bytes, size, at + 8));
		float min = f32_from_bits(min_bits), max = f32_from_bits(max_bits);
		if (components[0] != expected[i] || (size_t)dynamic + constant != bones)
			return Error{"VBR channel " + std::to_string(i) + " does not match its SQT output descriptor"};
		h.channels.push_back(Channel{dynamic, constant, components[0], min, max - min});
	}
	size_t cursor = data;
	for (const Channel &channel : h.channels) {
		for (size_t k = 0; k < channel.constant * channel.components; k++) {
			S3_TRY(const uint8_t *index, take(bytes, size, cursor, 1));
			cursor += 1;
			if (index[0] >= dictionary_count) return Error{"VBR constant dictionary index out of bounds"};
			S3_TRY(uint32_t value_bits, u32_at(bytes, size, dictionary + index[0] * 4));
			// Original scalar fmadds82E8A804..878, including constants.
			h.constants.push_back(std::fma(f32_from_bits(value_bits), channel.range, channel.min));
		}
	}
	S3_TRY(uint16_t runs, u16_at(bytes, size, header));
	h.constant_flags.reserve(3 * bones);
	for (size_t run = 0; run < runs; run++) {
		S3_TRY(const uint8_t *length, take(bytes, size, cursor, 1));
		cursor += 1;
		if (h.constant_flags.size() + length[0] > 3 * bones) return Error{"VBR constant-channel run exceeds channel count"};
		h.constant_flags.insert(h.constant_flags.end(), length[0], run % 2 == 1);
	}
	if (runs == 0 && h.constants.empty()) h.constant_flags.resize(3 * bones, false);
	if (h.constant_flags.size() != 3 * bones) return Error{"VBR constant-channel runs do not cover all channels"};
	for (size_t i = 0; i < 3; i++) {
		size_t n = 0;
		for (size_t b = i * bones; b < (i + 1) * bones; b++) n += h.constant_flags[b] ? 1 : 0;
		if (n != h.channels[i].constant) return Error{"VBR channel " + std::to_string(i) + " constant count differs from its runs"};
	}
	size_t dynamic_count = 0;
	for (const Channel &c : h.channels) dynamic_count += c.dynamic * c.components;
	h.widths.reserve(dynamic_count);
	for (size_t k = 0; k < dynamic_count; k++) {
		S3_TRY(uint32_t w, u32_at(bytes, size, cursor));
		h.widths.push_back(w);
		cursor += 4;
	}
	size_t block_start = cursor;
	// Vector_ExtractPackedNVBR82D1E0E4..118 accumulates u16 block sizes.
	uint16_t displacement = 0;
	for (size_t block = 0; block < block_count; block++) {
		S3_TRY(uint16_t bsize, u16_at(bytes, size, size_table + 2 * block));
		S3_TRY(size_t start, offset(block_start, displacement));
		S3_TRY(size_t end, offset(start, bsize));
		if (end > data_end || end < start || bsize < 3) return Error{"VBR block " + std::to_string(block) + " exceeds compressed data"};
		h.blocks.push_back(BlockRange{start, end});
		displacement = (uint16_t)(displacement + bsize);
	}
	S3_TRY(uint32_t low_bits, u32_at(bytes, size, header + 4));
	S3_TRY(uint32_t high_bits, u32_at(bytes, size, header + 8));
	float low = std::fabs(f32_from_bits(low_bits));
	float high = std::fabs(f32_from_bits(high_bits));
	// Rust f32::max ignores a NaN operand.
	h.dct_scale = std::isnan(low) ? high : std::isnan(high) ? low : (low > high ? low : high);
	return h;
}

// ---- bits.rs ----
//! The native reverse-byte permutation830CEAD0 (initializer82F96DF8) makes
//! both streams little endian. Width nibbles come from big-endian header words.

namespace {
struct Bits
{
	const uint8_t *bytes;
	size_t size;
	size_t position = 0;
	Result<uint32_t> read(uint32_t count)
	{
		uint32_t value = 0;
		for (uint32_t bit = 0; bit < count; bit++) {
			if (position / 8 >= size) return Error{"VBR coefficient bit stream truncated"};
			uint8_t byte = bytes[position / 8];
			value |= (uint32_t)((byte >> (position % 8)) & 1) << bit;
			position++;
		}
		return value;
	}
};
} // namespace

Result<std::vector<std::array<float, 8>>> unpack(const uint8_t *block, size_t size, const std::vector<uint32_t> &widths)
{
	std::vector<std::array<float, 8>> output;
	if (widths.empty()) return output;
	if (size < 1) return Error{"VBR block has no selector length"};
	size_t prefix = block[0];
	size_t mag_start = 1 + (prefix + 7) / 8;
	if (mag_start > size) return Error{"VBR selector length exceeds block"};
	Bits selected{block + 1, size - 1};
	Bits packed{block + mag_start, size - mag_start};
	output.reserve(widths.size());
	for (uint32_t descriptor : widths) {
		std::array<float, 8> coefficients{};
		for (size_t i = 0; i < 8; i++) {
			uint32_t width = (descriptor >> (4 * i)) & 15;
			// A zero width consumes neither a selector nor packed data.
			if (width == 0) continue;
			S3_TRY(uint32_t sel, selected.read(1));
			if (sel == 0) continue;
			S3_TRY(uint32_t positive, packed.read(1));
			S3_TRY(uint32_t magnitude_int, packed.read(width));
			float magnitude = (float)magnitude_int;
			// The original complements the sign bit, then shifts it to
			// bit31 and ORs it into the converted unsigned magnitude.
			// Preserve negative zero; this is not two's complement.
			coefficients[i] = f32_from_bits(f32_to_bits(magnitude) | (positive != 0 ? 0u : 0x80000000u));
		}
		output.push_back(coefficients);
	}
	return output;
}

// ---- trigonometry::cos ----

float trig_cos(float angle)
{
	float turns = std::nearbyint(angle * f32_from_bits(0x3e22f983u)); // round_ties_even
	float x = std::fma(-f32_from_bits(0x40c90fdbu), turns, angle);
	float x2 = x * x;
	float x4 = x2 * x2;
	float x6 = x4 * x2;
	float x8 = x4 * x4;
	float x10 = x6 * x4;
	float x12 = x6 * x6;
	float x14 = x8 * x6;
	float x16 = x8 * x8;
	float x18 = x10 * x8;
	float x22 = x12 * x10;
	float x20 = x10 * x10;
	float value = std::fma(-0.5f, x2, 1.0f);
	const std::pair<float, uint32_t> terms[] = {
		{x4, 0x3d2aaaab}, {x6, 0xbab60b61}, {x8, 0x37d00d01}, {x10, 0xb493f27e}, {x12, 0x310f76c8},
		{x14, 0xad49cba5}, {x16, 0x29573f9f}, {x18, 0xa53413c3}, {x20, 0x20f2a15d}, {x22, 0x9c8671cb},
	};
	for (auto &t : terms) value = std::fma(f32_from_bits(t.second), t.first, value);
	return value;
}

// ---- transform.rs ----

namespace {
/// Original logarithm polynomial82E89598..5DC, evaluated only at integers2..9.
float logarithm_for_frequency(uint32_t integer)
{
	float input = (float)integer;
	int32_t exponent = (int32_t)((f32_to_bits(input) >> 23) & 255) - 127;
	float mantissa = f32_from_bits((f32_to_bits(input) & 0x007fffffu) | 0x3f800000u);
	float a = mantissa - 1.0f;
	float a2 = a * a;
	float a3 = a * a2;
	float a4 = a2 * a2;
	// Original vectors8230EA70 and8230EA80.
	static const uint32_t cw[8] = {0x3fb8aa0e, 0xbf389e52, 0x3ef5162d, 0xbeb1d204, 0x3e77adbd, 0xbe0cd4fb, 0x3d5541c6, 0xbc188b0b};
	float c[8];
	for (int i = 0; i < 8; i++) c[i] = f32_from_bits(cw[i]);
	float low = std::fma(a, c[1], c[0]);
	float high = std::fma(a, c[5], c[4]);
	low = std::fma(a2, c[2], low);
	high = std::fma(a2, c[6], high);
	low = std::fma(a3, c[3], low);
	high = std::fma(a3, c[7], high);
	float polynomial = std::fma(a4, high, low);
	return std::fma(a, polynomial, (float)exponent) * f32_from_bits(0x3f317218u); //82139A70
}

struct Tables
{
	float quantization[256][8];
	float inverse_dct[8][8];
	Tables()
	{
		float base[8];
		for (uint32_t i = 0; i < 8; i++) base[i] = logarithm_for_frequency(i + 2) * f32_from_bits(0x38000000u);
		float multiplier = 1.0f;
		for (int r = 0; r < 256; r++) {
			for (int i = 0; i < 8; i++) quantization[r][i] = base[i] * multiplier;
			multiplier += f32_from_bits(0x3e4ccccdu); // 82099280; accumulated fadds
		}
		// 822F8B48..58 and individual .785398/.1.570796 literal loads.
		static const uint32_t angles[8] = {0, 0x3ec90fdb, 0x3f490fdb, 0x3f96cbe4, 0x3fc90fdb, 0x3ffb53d2, 0x4016cbe4, 0x402fede0};
		for (int frame = 0; frame < 8; frame++) {
			float phase = (float)frame + 0.5f;
			for (int i = 0; i < 8; i++) inverse_dct[frame][i] = trig_cos(phase * f32_from_bits(angles[i])) * 0.25f;
			inverse_dct[frame][0] *= 0.5f;
		}
	}
};
const Tables &tables()
{
	static const Tables t;
	return t;
}

// physics::native_arithmetic::dot4: dot3 + l3*r3, unfused.
float dot4(const float *l, const float *r)
{
	float d3 = l[0] * r[0] + l[1] * r[1] + l[2] * r[2];
	return d3 + l[3] * r[3];
}
} // namespace

float decode(size_t frame, uint8_t quantization_index, float scale, const std::array<float, 8> &coefficients, float channel_min,
             float channel_range)
{
	const Tables &t = tables();
	float squared = scale * scale;
	// Literal820612E0 is 2^-15. This equation was independently recovered
	// from original vmulfp/vmaddcfp instructions82E89930..4C.
	float bias = scale * f32_from_bits(0x38000000u);
	float weights[8];
	for (int i = 0; i < 8; i++) weights[i] = std::fma(t.quantization[quantization_index][i], squared, bias) * t.inverse_dct[frame][i];
	float first = dot4(coefficients.data(), weights);
	float last = dot4(coefficients.data() + 4, weights + 4);
	// BSS830CEAF0 initializer82F96D58 converts integer1 with scale1 -> .5.
	return std::fma((first + last) + 0.5f, channel_range, channel_min);
}

// ---- mod.rs ----

Result<VbrDecoder> VbrDecoder::create(const uint8_t *bytes, size_t size, size_t part_offset, size_t compression_header_relative,
                                      size_t compressed_data_relative)
{
	VbrDecoder d;
	d.bytes_ = bytes;
	d.size_ = size;
	S3_TRY(d.header_, Header::read(bytes, size, part_offset, compression_header_relative, compressed_data_relative));
	return d;
}

Result<std::vector<SampleWords>> VbrDecoder::decode_frame(size_t frame)
{
	if (frame >= header_.frames) return Error{"VBR frame " + std::to_string(frame) + " out of range"};
	size_t block_index = frame / 8;
	const BlockRange &range = header_.blocks[block_index];
	const uint8_t *block = bytes_ + range.start;
	size_t block_size = range.end - range.start;
	if (!cached_block_ || cached_block_->first != block_index) {
		S3_TRY(auto coefficients, unpack(block + 3, block_size - 3, header_.widths));
		cached_block_ = std::make_pair(block_index, std::move(coefficients));
	}
	const auto &coefficients = cached_block_->second;
	std::vector<SampleWords> output(header_.bones, SampleWords{});
	size_t constant = 0, dynamic = 0, component_offset = 0;
	for (size_t channel_index = 0; channel_index < header_.channels.size(); channel_index++) {
		const Channel &channel = header_.channels[channel_index];
		for (size_t bone = 0; bone < header_.bones; bone++) {
			bool is_constant = header_.constant_flags[channel_index * header_.bones + bone];
			for (size_t component = 0; component < channel.components; component++) {
				float value;
				if (is_constant) {
					if (constant >= header_.constants.size()) return Error{"VBR constant index out of range"};
					value = header_.constants[constant++];
				} else {
					if (dynamic >= coefficients.size() || channel_index >= block_size) return Error{"VBR coefficient index out of range"};
					value = decode(frame % 8, block[channel_index], header_.dct_scale, coefficients[dynamic], channel.min, channel.range);
					dynamic++;
				}
				output[bone][component_offset + component] = f32_to_bits(value);
			}
		}
		component_offset += channel.components;
	}
	return output;
}

} // namespace s3::data::vbr
