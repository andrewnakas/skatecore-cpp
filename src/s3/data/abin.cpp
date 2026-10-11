// Ordered bank indexing and lossless header decoding (abin/{mod,reader,bank}.rs).
#include "abin.hpp"

#include <limits>

namespace s3::data::abin {

std::string error_text(size_t offset, const std::string &message) { return "ABIN at " + hex_prefixed(offset) + ": " + message; }

Codec Codec::from_word(uint32_t word)
{
	Codec c;
	if (word == 0x00004452u) c.kind = Raw;
	else if (word == 0x00524256u) c.kind = Vbr;
	else {
		c.kind = Other;
		c.other = word;
	}
	return c;
}

std::string Codec::debug() const
{
	if (kind == Raw) return "Raw";
	if (kind == Vbr) return "Vbr";
	return "Other(" + std::to_string(other) + ")";
}

bool Hierarchy::compatible_with(const Hierarchy &other) const
{
	return bone_count == other.bone_count && has_trajectory == other.has_trajectory && parents == other.parents &&
	       mirrors == other.mirrors && bone_names == other.bone_names && parts == other.parts;
}

// ---- Reader ----

Result<Reader> Reader::bounded(Range range) const
{
	if (range.end < range.start) return error(range.start, "reversed record range");
	S3_CHECK(take(range.start, range.end - range.start));
	Reader r = *this;
	r.begin_ = range.start;
	r.end_ = range.end;
	return r;
}

Result<const uint8_t *> Reader::take(size_t offset, size_t size) const
{
	if (size > std::numeric_limits<size_t>::max() - offset) return error(offset, "read overflow");
	size_t end = offset + size;
	if (offset < begin_ || end > end_)
		return error(offset, "read size" + std::to_string(size) + " exceeds record " + hex_prefixed(begin_) + ".." + hex_prefixed(end_));
	return bytes_ + offset;
}

Result<uint8_t> Reader::u8(size_t offset) const
{
	S3_TRY(const uint8_t *p, take(offset, 1));
	return p[0];
}

Result<uint16_t> Reader::u16(size_t offset) const
{
	S3_TRY(const uint8_t *p, take(offset, 2));
	return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}

Result<uint32_t> Reader::u32(size_t offset) const
{
	S3_TRY(const uint8_t *p, take(offset, 4));
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

Result<int32_t> Reader::i32(size_t offset) const
{
	S3_TRY(uint32_t v, u32(offset));
	return (int32_t)v;
}

Result<std::string> Reader::name(size_t offset, size_t words) const
{
	if (words > std::numeric_limits<size_t>::max() / 4) return error(offset, "name size overflow");
	S3_TRY(const uint8_t *raw, take(offset, words * 4));
	static const char ALPHABET[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_";
	std::string result;
	for (size_t w = 0; w < words; w++) {
		const uint8_t *p = raw + 4 * w;
		uint32_t value = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
		uint32_t divisor = 38u * 38u * 38u * 38u * 38u;
		for (int i = 0; i < 6; i++) {
			uint32_t digit = value / divisor;
			value %= divisor;
			if (digit == 0) {
				if (value != 0) return error(offset, "invalid FastString padding");
				break;
			}
			if (digit - 1 >= 37) return error(offset, "invalid FastString digit");
			result += ALPHABET[digit - 1];
			divisor /= 38;
		}
	}
	return result;
}

Result<Range> Reader::relative(size_t base, uint32_t offset, size_t size) const
{
	if ((size_t)offset > std::numeric_limits<size_t>::max() - base) return error(base, "relative offset overflow");
	size_t start = base + offset;
	S3_CHECK(take(start, size));
	return Range{start, start + size};
}

// ---- Bank ----

static Result<std::vector<PartEntry>> read_parts(const Reader &r, size_t base, size_t table, size_t count, bool channel)
{
	if (count > std::numeric_limits<size_t>::max() / 4) return error(base, "part table size overflow");
	S3_CHECK(r.take(base + table, count * 4));
	std::vector<PartEntry> parts;
	parts.reserve(count);
	for (size_t i = 0; i < count; i++) {
		size_t table_offset = base + table + 4 * i;
		S3_TRY(uint32_t word, r.u32(table_offset));
		uint32_t relative = word & 0x00ffffffu;
		PartEntry entry;
		entry.table_index = i;
		entry.table_offset = table_offset;
		entry.raw_word = word;
		entry.encoded_id = (uint8_t)(word >> 24);
		if (relative != 0) {
			AnimationPart part;
			S3_TRY(Range at, r.relative(base, relative, 16));
			part.offset = at.start;
			S3_TRY(part.header_word, r.u32(part.offset));
			S3_TRY(part.compressed_size, r.u32(part.offset + 4));
			S3_TRY(part.compression_header_relative, r.u32(part.offset + 8));
			S3_TRY(part.compressed_data_relative, r.u32(part.offset + 12));
			part.channel_count = (uint16_t)(((part.header_word >> 6) & 63) + 1);
			S3_TRY(part.compression_header, r.relative(part.offset, part.compression_header_relative, (size_t)(part.header_word >> 12)));
			S3_TRY(part.compressed_data, r.relative(part.offset, part.compressed_data_relative, (size_t)part.compressed_size));
			if (channel) {
				S3_TRY(Range weights, r.relative(part.offset, 16, 4 * (size_t)part.channel_count));
				part.channel_weights = weights;
			}
			entry.part = part;
		}
		parts.push_back(std::move(entry));
	}
	return parts;
}

static Result<Clip> read_clip(const Reader &r, const RecordHeader &h)
{
	size_t p = h.payload_offset;
	S3_CHECK(r.take(p, 56));
	S3_TRY(uint32_t flags, r.u32(p + 52));
	Clip clip;
	clip.flags = flags;
	clip.attribute_count = (uint8_t)flags;
	S3_TRY(uint32_t attr_rel, r.u32(p + 44));
	S3_TRY(Range attr, r.relative(p, attr_rel, 0));
	clip.attribute_offset = attr.start;
	S3_TRY(clip.fps_bits, r.u32(p + 32));
	S3_TRY(clip.frame_count_bits, r.u32(p + 36));
	S3_TRY(clip.base_speed_bits, r.u32(p + 40));
	S3_TRY(clip.loop_translation_words, r.words<4>(p));
	S3_TRY(clip.loop_rotation_words, r.words<4>(p + 16));
	S3_TRY(clip.parts, read_parts(r, p, 56, (size_t)((flags >> 8) & 31), (flags & 0x20000000u) != 0));
	return clip;
}

static Result<Hierarchy> read_hierarchy(const Reader &r, size_t p)
{
	S3_TRY(uint16_t bone_count, r.u16(p));
	size_t n = bone_count;
	S3_TRY(uint32_t count32, r.u32(p + 4));
	size_t count = count32;
	if (count > std::numeric_limits<size_t>::max() / 36) return error(p, "hierarchy part count overflow");
	size_t part_bytes = count * 36;
	size_t part_base = p + 8 + 8 * n;
	S3_CHECK(r.take(p + 8, 8 * n));
	S3_CHECK(r.take(part_base, part_bytes));
	size_t name_base = part_base + part_bytes;
	S3_CHECK(r.take(name_base, 20 * n));
	Hierarchy h;
	h.offset = p;
	h.bone_count = bone_count;
	h.parents.reserve(n);
	h.mirrors.reserve(n);
	h.bone_names.reserve(n);
	for (size_t i = 0; i < n; i++) {
		S3_TRY(int32_t parent, r.i32(p + 8 + 4 * i));
		S3_TRY(int32_t mirror, r.i32(p + 8 + 4 * n + 4 * i));
		if (parent < -1 || parent >= (int32_t)i || mirror < -1 || mirror >= (int32_t)n)
			return error(p + 8 + 4 * i, "invalid parent or mirror index");
		h.parents.push_back(parent);
		h.mirrors.push_back(mirror);
		S3_TRY(std::string name, r.name(name_base + 20 * i, 5));
		h.bone_names.push_back(std::move(name));
	}
	h.parts.reserve(count);
	for (size_t i = 0; i < count; i++) {
		size_t at = part_base + i * 36;
		S3_TRY(uint32_t part_bones, r.u32(at + 24));
		S3_TRY(int32_t sqt_offset, r.i32(at + 28));
		if (sqt_offset < 0 || (size_t)sqt_offset + (size_t)part_bones > n) return error(at, "part extends outside skeleton output");
		HierarchyPart part;
		S3_TRY(part.name, r.name(at, 6));
		part.bone_count = part_bones;
		part.sqt_offset = sqt_offset;
		S3_TRY(part.flags, r.u16(at + 32));
		S3_TRY(part.encoded_id, r.u16(at + 34));
		h.parts.push_back(std::move(part));
	}
	S3_TRY(uint16_t trajectory, r.u16(p + 2));
	h.has_trajectory = trajectory != 0;
	return h;
}

Result<Bank> Bank::load(const Vfs &vfs, const std::string &path)
{
	S3_TRY_MAP(SharedBytes bytes, vfs.read(path), [&](const std::string &e) { return error_text(0, path + ": " + e); });
	return parse(std::move(bytes));
}

Result<Bank> Bank::parse(SharedBytes bytes)
{
	Reader reader(bytes.data, bytes.size);
	S3_TRY(uint32_t end32, reader.u32(0));
	size_t end = end32;
	S3_TRY(uint32_t kind, reader.u32(4));
	if (kind != 1 || end < 48 || end > bytes.size) return error(0, "invalid type1 bank wrapper");
	Bank bank;
	size_t at = 48;
	while (at < end) {
		S3_TRY(uint32_t size32, reader.u32(at));
		size_t size = size32;
		if (size < 48 || size % 16 != 0 || size > end - at) return error(at, "invalid record size/alignment");
		S3_TRY(Reader r, reader.bounded(Range{at, at + size}));
		RecordHeader header;
		S3_TRY(header.name, r.name(at + 16, 6));
		S3_TRY(header.name_words, r.words<6>(at + 16));
		header.offset = at;
		header.size = size;
		header.payload_offset = (at + 55) & ~(size_t)15;
		S3_TRY(header.type_id, r.u32(at + 4));
		S3_TRY(header.codec_word, r.u32(at + 8));
		S3_TRY(header.guid, r.u32(at + 12));
		RecordData data = Opaque{};
		switch (header.type_id) {
		case 2: {
			S3_TRY(Clip clip, read_clip(r, header));
			data = std::move(clip);
			break;
		}
		case 3: {
			S3_TRY(uint32_t count, r.u32(header.payload_offset));
			S3_TRY(std::vector<PartEntry> parts, read_parts(r, header.payload_offset, 4, count, false));
			data = Pose{std::move(parts)};
			break;
		}
		case 4: {
			S3_TRY(Hierarchy h, read_hierarchy(r, header.payload_offset));
			data = std::move(h);
			break;
		}
		case 5: {
			S3_TRY(uint32_t n, r.u32(header.payload_offset));
			if ((size_t)n > std::numeric_limits<size_t>::max() / 112) return error(at, "physics bone size overflow");
			S3_TRY(Range records, r.relative(header.payload_offset, 16, (size_t)n * 112));
			data = PhysicsPose{n, records};
			break;
		}
		default: break;
		}
		size_t index = bank.records_.size();
		// SetDBContent82D1B428..45C overwrites map entries in file order.
		// Clips also participate in the animation-tree map, poses do not.
		switch (header.type_id) {
		case 2:
			bank.clips_[header.name] = index;
			bank.animations_[header.name] = index;
			break;
		case 3: bank.poses_[header.name] = index; break;
		case 4: bank.hierarchy_ = index; break;
		case 6: case 7: case 8: case 9: case 10: case 11: bank.animations_[header.name] = index; break;
		default: break;
		}
		bank.offsets_[at] = index;
		bank.records_.push_back(Record{std::move(header), std::move(data)});
		at += size;
	}
	bank.wrapper_bytes = end;
	bank.trailer = Range{end, bytes.size};
	bank.bytes_ = std::move(bytes);
	return bank;
}

const Record *Bank::record_at(size_t offset) const
{
	auto it = offsets_.find(offset);
	return it == offsets_.end() ? nullptr : &records_[it->second];
}

const Record *Bank::animation(const std::string &name) const
{
	auto it = animations_.find(to_ascii_uppercase(name));
	return it == animations_.end() ? nullptr : &records_[it->second];
}

std::pair<const RecordHeader *, const Clip *> Bank::clip(const std::string &name) const
{
	auto it = clips_.find(to_ascii_uppercase(name));
	if (it == clips_.end()) return {nullptr, nullptr};
	const Record &record = records_[it->second];
	if (auto *c = std::get_if<Clip>(&record.data)) return {&record.header, c};
	return {nullptr, nullptr};
}

std::pair<const RecordHeader *, const Pose *> Bank::pose(const std::string &name) const
{
	auto it = poses_.find(to_ascii_uppercase(name));
	if (it == poses_.end()) return {nullptr, nullptr};
	const Record &record = records_[it->second];
	if (auto *p = std::get_if<Pose>(&record.data)) return {&record.header, p};
	return {nullptr, nullptr};
}

const Hierarchy *Bank::hierarchy() const
{
	if (!hierarchy_) return nullptr;
	return std::get_if<Hierarchy>(&records_[*hierarchy_].data);
}

Status Bank::validate_positional_parts() const
{
	const Hierarchy *h = hierarchy();
	if (!h) return error(0, "missing hierarchy");
	for (const Record &record : records_) {
		const std::vector<PartEntry> *parts;
		if (auto *c = std::get_if<Clip>(&record.data)) parts = &c->parts;
		else if (auto *p = std::get_if<Pose>(&record.data)) parts = &p->parts;
		else continue;
		if (parts->size() != h->parts.size()) return error(record.header.offset, "part table differs from full hierarchy");
		for (size_t i = 0; i < parts->size(); i++) {
			const PartEntry &entry = (*parts)[i];
			if (entry.part && (uint32_t)entry.part->channel_count != h->parts[i].bone_count)
				return error(entry.part->offset, "part channel count differs from hierarchy");
		}
	}
	return Unit{};
}

} // namespace s3::data::abin
