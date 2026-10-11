// Direct stock VBR SQT decoding from original Skate 3 TU3.
// Source:82E88FA8/82E8A780(header),82E8A928(bits),82E89770(transform),
// 82E89A60/82E88BA0(output), and82D20298(SQT channel descriptors).
// Host allocation/cache ownership is independent. PC floating-point arithmetic
// is not claimed to emulate the Xenon's numerical instructions bit for bit.
// (skate-core animation/vbr/{mod,bits,header,transform}.rs, needed by
// skate-data animation_frames; lives here so the data stage is self-contained.
// Namespace s3::data::vbr; the core stage may alias it as s3::animation::vbr.)
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "result.hpp"

namespace s3::data::vbr {

struct Channel
{
	size_t dynamic = 0;
	size_t constant = 0;
	size_t components = 0;
	float min = 0;
	float range = 0;
};

struct BlockRange
{
	size_t start = 0, end = 0;
};

//! Original TU3 InitPerAnim82E88FA8 and UnPackHeaderBits82E8A780.
struct Header
{
	size_t frames = 0;
	size_t bones = 0;
	float dct_scale = 0;
	std::vector<Channel> channels;
	std::vector<float> constants;
	std::vector<bool> constant_flags;
	std::vector<uint32_t> widths;
	std::vector<BlockRange> blocks;
	static Result<Header> read(const uint8_t *bytes, size_t size, size_t part, size_t header_relative, size_t data_relative);
};

//! TU3 Vector_UnPackFrameBlock82E8A928, expressed as scalar bit readers.
Result<std::vector<std::array<float, 8>>> unpack(const uint8_t *block, size_t size, const std::vector<uint32_t> &widths);

//! TU3 Init82E891D8, IDCT table82E88C58, DecompressFrameBlock82E89770.
float decode(size_t frame, uint8_t quantization_index, float scale, const std::array<float, 8> &coefficients, float channel_min,
             float channel_range);

// skate-core trigonometry::cos (standalone TU3 cosine 82473930).
float trig_cos(float angle);

using SampleWords = std::array<uint32_t, 10>;

class VbrDecoder
{
public:
	static Result<VbrDecoder> create(const uint8_t *bytes, size_t size, size_t part_offset, size_t compression_header_relative,
	                                 size_t compressed_data_relative);
	size_t frame_count() const { return header_.frames; }
	size_t channel_count() const { return header_.bones; }
	///Return each authored bone's scaleXYZ, quaternionXYZW, translationXYZ bits.
	///No quaternion normalization, retargeting, hierarchy changes or weight edits.
	Result<std::vector<SampleWords>> decode_frame(size_t frame);

private:
	const uint8_t *bytes_ = nullptr;
	size_t size_ = 0;
	Header header_;
	std::optional<std::pair<size_t, std::vector<std::array<float, 8>>>> cached_block_;
};

} // namespace s3::data::vbr
