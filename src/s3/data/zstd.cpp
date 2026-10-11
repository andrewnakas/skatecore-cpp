// Zstandard frame decoder (RFC 8878), structured after the reference educational decoder.
#include "zstd.hpp"

#include <cstring>

namespace s3::data::zstd {

static inline uint64_t rotl64(uint64_t x, int r) { return (x << r) | (x >> (64 - r)); }
static inline uint64_t rd64(const uint8_t *p)
{
	uint64_t v = 0;
	for (int i = 7; i >= 0; i--) v = v << 8 | p[i];
	return v;
}
static inline uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

uint64_t xxh64(const uint8_t *p, size_t len, uint64_t seed)
{
	const uint64_t P1 = 0x9E3779B185EBCA87ull, P2 = 0xC2B2AE3D27D4EB4Full, P3 = 0x165667B19E3779F9ull, P4 = 0x85EBCA77C2B2AE63ull,
	               P5 = 0x27D4EB2F165667C5ull;
	auto round = [&](uint64_t acc, uint64_t input) {
		acc += input * P2;
		acc = rotl64(acc, 31);
		return acc * P1;
	};
	auto merge = [&](uint64_t acc, uint64_t val) {
		val = round(0, val);
		acc ^= val;
		return acc * P1 + P4;
	};
	const uint8_t *end = p + len;
	uint64_t h;
	if (len >= 32) {
		uint64_t v1 = seed + P1 + P2, v2 = seed + P2, v3 = seed, v4 = seed - P1;
		const uint8_t *limit = end - 32;
		do {
			v1 = round(v1, rd64(p));
			v2 = round(v2, rd64(p + 8));
			v3 = round(v3, rd64(p + 16));
			v4 = round(v4, rd64(p + 24));
			p += 32;
		} while (p <= limit);
		h = rotl64(v1, 1) + rotl64(v2, 7) + rotl64(v3, 12) + rotl64(v4, 18);
		h = merge(h, v1);
		h = merge(h, v2);
		h = merge(h, v3);
		h = merge(h, v4);
	} else {
		h = seed + P5;
	}
	h += (uint64_t)len;
	while (p + 8 <= end) {
		h ^= round(0, rd64(p));
		h = rotl64(h, 27) * P1 + P4;
		p += 8;
	}
	if (p + 4 <= end) {
		h ^= (uint64_t)rd32(p) * P1;
		h = rotl64(h, 23) * P2 + P3;
		p += 4;
	}
	while (p < end) {
		h ^= (uint64_t)(*p) * P5;
		h = rotl64(h, 11) * P1;
		p++;
	}
	h ^= h >> 33;
	h *= P2;
	h ^= h >> 29;
	h *= P3;
	h ^= h >> 32;
	return h;
}

namespace {

int highest_set_bit(uint64_t v)
{
	int r = -1;
	while (v) {
		v >>= 1;
		r++;
	}
	return r;
}

// Little-endian bits starting at bit `pos`.
uint64_t read_bits_le(const uint8_t *src, int bits, size_t pos)
{
	if (bits == 0) return 0;
	uint64_t res = 0;
	int shift = 0;
	size_t byte = pos / 8;
	int bit = (int)(pos % 8);
	int left = bits;
	while (left > 0) {
		int take = 8 - bit < left ? 8 - bit : left;
		uint64_t chunk = (uint64_t)(src[byte] >> bit) & ((1u << take) - 1);
		res |= chunk << shift;
		shift += take;
		left -= take;
		byte++;
		bit = 0;
	}
	return res;
}

// Backward bitstream (reads from the end toward the start).
struct BackStream
{
	const uint8_t *src = nullptr;
	int64_t offset = 0;
	bool init(const uint8_t *s, size_t len)
	{
		src = s;
		if (len == 0 || s[len - 1] == 0) return false;
		int padding = 8 - highest_set_bit(s[len - 1]);
		offset = (int64_t)len * 8 - padding;
		return true;
	}
	uint64_t read(int bits)
	{
		if (bits == 0) return 0;
		offset -= bits;
		int64_t actual_off = offset;
		int actual_bits = bits;
		if (offset < 0) {
			actual_bits += (int)offset;
			actual_off = 0;
		}
		uint64_t res = actual_bits > 0 ? read_bits_le(src, actual_bits, (size_t)actual_off) : 0;
		if (offset < 0) res = -offset >= 64 ? 0 : (res << -offset);
		return res;
	}
};

// Forward bitstream for FSE table headers.
struct ForwardBits
{
	const uint8_t *src;
	size_t len;
	size_t bitpos = 0;
	bool ok = true;
	uint64_t read(int bits)
	{
		if (bits == 0) return 0;
		if (bitpos + (size_t)bits > len * 8) {
			ok = false;
			bitpos += bits;
			return 0;
		}
		uint64_t v = read_bits_le(src, bits, bitpos);
		bitpos += bits;
		return v;
	}
	void rewind(int bits) { bitpos -= bits; }
	size_t bytes_used() const { return (bitpos + 7) / 8; }
};

constexpr int FSE_MAX_ACCURACY = 9;

struct FseTable
{
	int accuracy_log = 0;
	uint8_t symbols[1 << FSE_MAX_ACCURACY];
	uint8_t num_bits[1 << FSE_MAX_ACCURACY];
	uint16_t new_state_base[1 << FSE_MAX_ACCURACY];
	bool valid = false;
};

bool fse_build(FseTable &t, const int16_t *freqs, int num_symbs, int accuracy_log)
{
	if (accuracy_log > FSE_MAX_ACCURACY || num_symbs > 256) return false;
	size_t size = (size_t)1 << accuracy_log;
	uint16_t state_desc[256];
	size_t high_threshold = size;
	for (int s = 0; s < num_symbs; s++) {
		if (freqs[s] == -1) {
			t.symbols[--high_threshold] = (uint8_t)s;
			state_desc[s] = 1;
		}
	}
	size_t step = (size >> 1) + (size >> 3) + 3, mask = size - 1, pos = 0;
	for (int s = 0; s < num_symbs; s++) {
		if (freqs[s] <= 0) continue;
		state_desc[s] = (uint16_t)freqs[s];
		for (int i = 0; i < freqs[s]; i++) {
			t.symbols[pos] = (uint8_t)s;
			do {
				pos = (pos + step) & mask;
			} while (pos >= high_threshold);
		}
	}
	if (pos != 0) return false;
	for (size_t i = 0; i < size; i++) {
		uint8_t symbol = t.symbols[i];
		uint16_t next = state_desc[symbol]++;
		if (next == 0) return false;
		t.num_bits[i] = (uint8_t)(accuracy_log - highest_set_bit(next));
		t.new_state_base[i] = (uint16_t)(((uint32_t)next << t.num_bits[i]) - size);
	}
	t.accuracy_log = accuracy_log;
	t.valid = true;
	return true;
}

void fse_rle(FseTable &t, uint8_t symbol)
{
	t.accuracy_log = 0;
	t.symbols[0] = symbol;
	t.num_bits[0] = 0;
	t.new_state_base[0] = 0;
	t.valid = true;
}

// Reads an FSE table description; returns bytes consumed or 0 on error.
size_t fse_read_header(FseTable &t, const uint8_t *src, size_t len, int max_accuracy, int max_symbs)
{
	ForwardBits in{src, len};
	int accuracy_log = 5 + (int)in.read(4);
	if (!in.ok || accuracy_log > max_accuracy) return 0;
	int16_t freqs[256] = {0};
	int32_t remaining = 1 << accuracy_log;
	int symb = 0;
	while (remaining > 0 && symb < max_symbs) {
		int bits = highest_set_bit((uint64_t)remaining + 1) + 1;
		uint32_t val = (uint32_t)in.read(bits);
		uint32_t lower_mask = (1u << (bits - 1)) - 1;
		uint32_t threshold = (1u << bits) - 1 - ((uint32_t)remaining + 1);
		if ((val & lower_mask) < threshold) {
			in.rewind(1);
			val &= lower_mask;
		} else if (val > lower_mask) {
			val -= threshold;
		}
		int proba = (int)val - 1;
		remaining -= proba < 0 ? -proba : proba;
		freqs[symb++] = (int16_t)proba;
		if (proba == 0) {
			uint32_t repeat = (uint32_t)in.read(2);
			for (;;) {
				for (uint32_t i = 0; i < repeat && symb < max_symbs; i++) freqs[symb++] = 0;
				if (repeat == 3) repeat = (uint32_t)in.read(2);
				else break;
			}
		}
		if (!in.ok) return 0;
	}
	if (remaining != 0 || !in.ok) return 0;
	if (!fse_build(t, freqs, symb, accuracy_log)) return 0;
	return in.bytes_used();
}

struct FseState
{
	const FseTable *table;
	uint32_t state;
	void init(BackStream &s) { state = (uint32_t)s.read(table->accuracy_log); }
	uint8_t peek() const { return table->symbols[state]; }
	void update(BackStream &s) { state = table->new_state_base[state] + (uint32_t)s.read(table->num_bits[state]); }
};

constexpr int HUF_MAX_BITS = 11;

struct HufTable
{
	int max_bits = 0;
	uint8_t symbols[1 << HUF_MAX_BITS];
	uint8_t num_bits[1 << HUF_MAX_BITS];
	bool valid = false;
};

bool huf_build_from_weights(HufTable &t, const uint8_t *weights, int num_weights)
{
	// Last weight is implied.
	uint64_t weight_sum = 0;
	for (int i = 0; i < num_weights; i++) {
		if (weights[i] > HUF_MAX_BITS) return false;
		if (weights[i] > 0) weight_sum += (uint64_t)1 << (weights[i] - 1);
	}
	if (weight_sum == 0) return false;
	int max_bits = highest_set_bit(weight_sum) + 1;
	uint64_t left_over = ((uint64_t)1 << max_bits) - weight_sum;
	if (left_over & (left_over - 1)) return false;
	int last_weight = highest_set_bit(left_over) + 1;
	if (max_bits > HUF_MAX_BITS) return false;
	int num_symbs = num_weights + 1;
	if (num_symbs > 256) return false;
	uint8_t bits[256] = {0};
	for (int i = 0; i < num_weights; i++) bits[i] = weights[i] > 0 ? (uint8_t)(max_bits + 1 - weights[i]) : 0;
	bits[num_weights] = (uint8_t)(max_bits + 1 - last_weight);
	uint32_t rank_count[HUF_MAX_BITS + 2] = {0};
	for (int i = 0; i < num_symbs; i++) {
		if (bits[i] > max_bits) return false;
		rank_count[bits[i]]++;
	}
	uint32_t rank_idx[HUF_MAX_BITS + 2] = {0};
	size_t table_size = (size_t)1 << max_bits;
	rank_idx[max_bits] = 0;
	for (int i = max_bits; i >= 1; i--) {
		rank_idx[i - 1] = rank_idx[i] + rank_count[i] * (1u << (max_bits - i));
		if (rank_idx[i - 1] > table_size) return false;
		std::memset(&t.num_bits[rank_idx[i]], i, rank_idx[i - 1] - rank_idx[i]);
	}
	if (rank_idx[0] != table_size) return false;
	for (int i = 0; i < num_symbs; i++) {
		if (bits[i] == 0) continue;
		uint32_t code = rank_idx[bits[i]];
		uint32_t len = 1u << (max_bits - bits[i]);
		std::memset(&t.symbols[code], i, len);
		rank_idx[bits[i]] += len;
	}
	t.max_bits = max_bits;
	t.valid = true;
	return true;
}

struct Decoder
{
	std::vector<uint8_t> &out;
	size_t limit;
	size_t frame_start = 0;
	std::string error;
	bool stopped = false;
	// Frame state
	HufTable huf;
	FseTable ll, of, ml;
	uint64_t rep[3] = {1, 4, 8};
	std::vector<uint8_t> literals;

	Decoder(std::vector<uint8_t> &o, size_t l) : out(o), limit(l) {}

	bool fail(const char *message)
	{
		if (error.empty()) error = message;
		return false;
	}

	// Appends with the output limit; returns false once the limit is reached.
	bool emit(const uint8_t *p, size_t n)
	{
		size_t room = limit - out.size();
		if (n > room) {
			out.insert(out.end(), p, p + room);
			stopped = true;
			return false;
		}
		out.insert(out.end(), p, p + n);
		return true;
	}

	bool huf_read_table(const uint8_t *src, size_t len, size_t &used)
	{
		if (len < 1) return fail("truncated Huffman table");
		uint8_t header = src[0];
		uint8_t weights[256] = {0};
		int num_weights;
		if (header >= 128) {
			num_weights = header - 127;
			size_t bytes = ((size_t)num_weights + 1) / 2;
			if (1 + bytes > len) return fail("truncated Huffman weights");
			for (int i = 0; i < num_weights; i++)
				weights[i] = (uint8_t)(i % 2 == 0 ? src[1 + i / 2] >> 4 : src[1 + i / 2] & 15);
			used = 1 + bytes;
		} else {
			size_t csize = header;
			if (1 + csize > len) return fail("truncated Huffman weights");
			FseTable table;
			size_t hdr = fse_read_header(table, src + 1, csize, 6, 256);
			if (!hdr || hdr > csize) return fail("invalid Huffman weight table");
			BackStream bs;
			if (!bs.init(src + 1 + hdr, csize - hdr)) return fail("invalid Huffman weight stream");
			FseState s1{&table, 0}, s2{&table, 0};
			s1.init(bs);
			s2.init(bs);
			num_weights = 0;
			for (;;) {
				if (num_weights > 253) return fail("too many Huffman weights");
				weights[num_weights++] = s1.peek();
				s1.update(bs);
				if (bs.offset < 0) {
					weights[num_weights++] = s2.peek();
					break;
				}
				weights[num_weights++] = s2.peek();
				s2.update(bs);
				if (bs.offset < 0) {
					weights[num_weights++] = s1.peek();
					break;
				}
			}
			used = 1 + csize;
		}
		if (!huf_build_from_weights(huf, weights, num_weights)) return fail("invalid Huffman weights");
		return true;
	}

	bool huf_stream(const uint8_t *src, size_t len, uint8_t *dst, size_t count)
	{
		BackStream bs;
		if (!bs.init(src, len)) return fail("invalid Huffman stream");
		uint32_t mask = (1u << huf.max_bits) - 1;
		uint32_t state = (uint32_t)bs.read(huf.max_bits);
		for (size_t i = 0; i < count; i++) {
			dst[i] = huf.symbols[state];
			int bits = huf.num_bits[state];
			state = ((state << bits) + (uint32_t)bs.read(bits)) & mask;
		}
		// The final state carried max_bits that were never consumed.
		if (bs.offset != -(int64_t)huf.max_bits) return fail("Huffman stream size mismatch");
		return true;
	}

	bool literals_section(const uint8_t *src, size_t len, size_t &used)
	{
		if (len < 1) return fail("truncated literals header");
		int type = src[0] & 3;
		int size_format = (src[0] >> 2) & 3;
		size_t regen, csize = 0, header;
		if (type <= 1) {
			if (size_format == 0 || size_format == 2) {
				header = 1;
				regen = src[0] >> 3;
			} else if (size_format == 1) {
				if (len < 2) return fail("truncated literals header");
				header = 2;
				regen = (src[0] >> 4) + ((size_t)src[1] << 4);
			} else {
				if (len < 3) return fail("truncated literals header");
				header = 3;
				regen = (src[0] >> 4) + ((size_t)src[1] << 4) + ((size_t)src[2] << 12);
			}
			literals.resize(regen);
			if (type == 0) {
				if (header + regen > len) return fail("truncated raw literals");
				if (regen) std::memcpy(literals.data(), src + header, regen);
				used = header + regen;
			} else {
				if (header + 1 > len) return fail("truncated RLE literals");
				std::memset(literals.data(), src[header], regen);
				used = header + 1;
			}
			return true;
		}
		int streams = size_format == 0 ? 1 : 4;
		if (size_format <= 1) {
			if (len < 3) return fail("truncated literals header");
			uint32_t h = src[0] | (uint32_t)src[1] << 8 | (uint32_t)src[2] << 16;
			header = 3;
			regen = (h >> 4) & 0x3FF;
			csize = (h >> 14) & 0x3FF;
		} else if (size_format == 2) {
			if (len < 4) return fail("truncated literals header");
			uint32_t h = rd32(src);
			header = 4;
			regen = (h >> 4) & 0x3FFF;
			csize = (h >> 18) & 0x3FFF;
		} else {
			if (len < 5) return fail("truncated literals header");
			uint64_t h = rd32(src) | (uint64_t)src[4] << 32;
			header = 5;
			regen = (size_t)((h >> 4) & 0x3FFFF);
			csize = (size_t)((h >> 22) & 0x3FFFF);
		}
		if (header + csize > len) return fail("truncated compressed literals");
		const uint8_t *p = src + header;
		size_t remaining = csize;
		if (type == 2) {
			size_t table_bytes = 0;
			if (!huf_read_table(p, remaining, table_bytes)) return false;
			p += table_bytes;
			remaining -= table_bytes;
		} else if (!huf.valid) {
			return fail("treeless literals without a previous table");
		}
		literals.resize(regen);
		if (streams == 1) {
			if (!huf_stream(p, remaining, literals.data(), regen)) return false;
		} else {
			if (remaining < 6) return fail("truncated jump table");
			size_t s1 = p[0] | (size_t)p[1] << 8, s2 = p[2] | (size_t)p[3] << 8, s3 = p[4] | (size_t)p[5] << 8;
			if (6 + s1 + s2 + s3 > remaining) return fail("invalid jump table");
			size_t s4 = remaining - 6 - s1 - s2 - s3;
			size_t per = (regen + 3) / 4;
			if (3 * per > regen) return fail("invalid literal stream sizes");
			const uint8_t *q = p + 6;
			size_t sizes[4] = {s1, s2, s3, s4};
			size_t counts[4] = {per, per, per, regen - 3 * per};
			size_t at = 0;
			for (int i = 0; i < 4; i++) {
				if (!huf_stream(q, sizes[i], literals.data() + at, counts[i])) return false;
				q += sizes[i];
				at += counts[i];
			}
		}
		used = header + csize;
		return true;
	}

	bool table_for(FseTable &t, int mode, const uint8_t *&p, const uint8_t *end, const int16_t *predefined, int predefined_symbs,
	               int predefined_log, int max_log, int max_symbs)
	{
		switch (mode) {
		case 0:
			if (!fse_build(t, predefined, predefined_symbs, predefined_log)) return fail("invalid predefined table");
			return true;
		case 1:
			if (p >= end) return fail("truncated RLE table");
			if (*p >= max_symbs) return fail("invalid RLE symbol");
			fse_rle(t, *p++);
			return true;
		case 2: {
			size_t used = fse_read_header(t, p, (size_t)(end - p), max_log, max_symbs);
			if (!used) return fail("invalid FSE table");
			p += used;
			return true;
		}
		default:
			if (!t.valid) return fail("repeat table without a previous table");
			return true;
		}
	}

	bool block(const uint8_t *src, size_t len)
	{
		size_t used = 0;
		if (!literals_section(src, len, used)) return false;
		const uint8_t *p = src + used, *end = src + len;
		if (p >= end) return fail("missing sequences section");
		size_t nb_seq = *p++;
		if (nb_seq >= 128) {
			if (nb_seq < 255) {
				if (p >= end) return fail("truncated sequence count");
				nb_seq = ((nb_seq - 128) << 8) + *p++;
			} else {
				if (end - p < 2) return fail("truncated sequence count");
				nb_seq = p[0] + ((size_t)p[1] << 8) + 0x7F00;
				p += 2;
			}
		}
		size_t lit_at = 0;
		if (nb_seq == 0) {
			if (p != end) return fail("trailing data after an empty sequences section");
			emit(literals.data(), literals.size());
			return true;
		}
		if (p >= end) return fail("truncated compression modes");
		uint8_t modes = *p++;
		if (modes & 3) return fail("reserved compression mode bits");
		static const int16_t LL_DEFAULT[36] = {4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 1, 1, 1, 1, 1, -1, -1, -1, -1};
		static const int16_t OF_DEFAULT[29] = {1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1};
		static const int16_t ML_DEFAULT[53] = {1, 4, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		                                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1};
		if (!table_for(ll, modes >> 6, p, end, LL_DEFAULT, 36, 6, 9, 36)) return false;
		if (!table_for(of, (modes >> 4) & 3, p, end, OF_DEFAULT, 29, 5, 8, 32)) return false;
		if (!table_for(ml, (modes >> 2) & 3, p, end, ML_DEFAULT, 53, 6, 9, 53)) return false;
		static const uint32_t LL_BASE[36] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18,
		                                     20, 22, 24, 28, 32, 40, 48, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};
		static const uint8_t LL_BITS[36] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
		static const uint32_t ML_BASE[53] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
		                                     21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 39, 41,
		                                     43, 47, 51, 59, 67, 83, 99, 131, 259, 515, 1027, 2051, 4099, 8195, 16387, 32771, 65539};
		static const uint8_t ML_BITS[53] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		                                    0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
		BackStream bs;
		if (!bs.init(p, (size_t)(end - p))) return fail("invalid sequence bitstream");
		FseState lls{&ll, 0}, ofs{&of, 0}, mls{&ml, 0};
		lls.init(bs);
		ofs.init(bs);
		mls.init(bs);
		for (size_t i = 0; i < nb_seq; i++) {
			uint8_t of_code = ofs.peek(), ll_code = lls.peek(), ml_code = mls.peek();
			if (ll_code > 35 || ml_code > 52 || of_code > 31) return fail("invalid sequence code");
			uint64_t offset_value = ((uint64_t)1 << of_code) + bs.read(of_code);
			size_t match_length = ML_BASE[ml_code] + (size_t)bs.read(ML_BITS[ml_code]);
			size_t literal_length = LL_BASE[ll_code] + (size_t)bs.read(LL_BITS[ll_code]);
			if (i + 1 < nb_seq) {
				lls.update(bs);
				mls.update(bs);
				ofs.update(bs);
			}
			if (bs.offset < 0) return fail("sequence bitstream overflow");
			// Repeat offsets.
			uint64_t offset;
			if (offset_value <= 3) {
				uint32_t idx = (uint32_t)offset_value - 1;
				if (literal_length == 0) idx++;
				if (idx == 0) {
					offset = rep[0];
				} else {
					offset = idx < 3 ? rep[idx] : rep[0] - 1;
					if (idx > 1) rep[2] = rep[1];
					rep[1] = rep[0];
					rep[0] = offset;
				}
			} else {
				offset = offset_value - 3;
				rep[2] = rep[1];
				rep[1] = rep[0];
				rep[0] = offset;
			}
			if (literal_length > literals.size() - lit_at) return fail("literal length exceeds literals");
			if (!emit(literals.data() + lit_at, literal_length)) return true;
			lit_at += literal_length;
			size_t produced = out.size() - frame_start;
			if (offset == 0 || offset > produced) return fail("match offset outside the window");
			for (size_t k = 0; k < match_length; k++) {
				if (out.size() >= limit) {
					stopped = true;
					return true;
				}
				out.push_back(out[out.size() - (size_t)offset]);
			}
		}
		if (bs.offset != 0) return fail("sequence bitstream size mismatch");
		emit(literals.data() + lit_at, literals.size() - lit_at);
		return true;
	}

	// Returns bytes consumed (0 on error); `stopped` when the limit was reached.
	size_t frame(const uint8_t *src, size_t len)
	{
		if (len < 4) return fail("truncated frame"), 0;
		uint32_t magic = rd32(src);
		if ((magic & 0xFFFFFFF0u) == 0x184D2A50u) {
			if (len < 8) return fail("truncated skippable frame"), 0;
			uint32_t n = rd32(src + 4);
			if ((uint64_t)n + 8 > len) return fail("truncated skippable frame"), 0;
			return 8 + (size_t)n;
		}
		if (magic != 0xFD2FB528u) return fail("unknown frame magic"), 0;
		size_t at = 4;
		if (at >= len) return fail("truncated frame header"), 0;
		uint8_t fhd = src[at++];
		int fcs_flag = fhd >> 6;
		bool single_segment = (fhd >> 5) & 1;
		bool checksum = (fhd >> 2) & 1;
		int dict_flag = fhd & 3;
		if (fhd & 8) return fail("reserved frame header bit"), 0;
		if (!single_segment) {
			if (at >= len) return fail("truncated frame header"), 0;
			at++; // window descriptor (the whole output stays addressable here)
		}
		static const int DICT_BYTES[4] = {0, 1, 2, 4};
		if (at + DICT_BYTES[dict_flag] > len) return fail("truncated frame header"), 0;
		uint32_t dict_id = 0;
		for (int i = 0; i < DICT_BYTES[dict_flag]; i++) dict_id |= (uint32_t)src[at + i] << (8 * i);
		at += DICT_BYTES[dict_flag];
		if (dict_id != 0) return fail("dictionaries are not supported"), 0;
		int fcs_bytes = fcs_flag == 0 ? (single_segment ? 1 : 0) : fcs_flag == 1 ? 2 : fcs_flag == 2 ? 4 : 8;
		if (at + fcs_bytes > len) return fail("truncated frame header"), 0;
		uint64_t fcs = 0;
		for (int i = 0; i < fcs_bytes; i++) fcs |= (uint64_t)src[at + i] << (8 * i);
		if (fcs_bytes == 2) fcs += 256;
		at += fcs_bytes;
		frame_start = out.size();
		huf.valid = ll.valid = of.valid = ml.valid = false;
		rep[0] = 1, rep[1] = 4, rep[2] = 8;
		for (;;) {
			if (at + 3 > len) return fail("truncated block header"), 0;
			uint32_t bh = src[at] | (uint32_t)src[at + 1] << 8 | (uint32_t)src[at + 2] << 16;
			at += 3;
			bool last = bh & 1;
			int type = (bh >> 1) & 3;
			size_t bsize = bh >> 3;
			if (type == 3) return fail("reserved block type"), 0;
			if (bsize > (128u << 10)) return fail("block too large"), 0;
			if (type == 1) {
				if (at + 1 > len) return fail("truncated RLE block"), 0;
				uint8_t v = src[at++];
				size_t room = limit - out.size();
				size_t n = bsize < room ? bsize : room;
				out.insert(out.end(), n, v);
				if (n < bsize) return stopped = true, at;
			} else {
				if (at + bsize > len) return fail("truncated block"), 0;
				if (type == 0) {
					if (!emit(src + at, bsize)) return at;
				} else {
					if (!block(src + at, bsize)) return 0;
					if (stopped) return at;
				}
				at += bsize;
			}
			if (last) break;
		}
		if (fcs_flag != 0 || single_segment) {
			if (out.size() - frame_start != fcs) return fail("frame content size mismatch"), 0;
		}
		if (checksum) {
			if (at + 4 > len) return fail("truncated checksum"), 0;
			uint32_t expected = rd32(src + at);
			uint32_t actual = (uint32_t)xxh64(out.data() + frame_start, out.size() - frame_start, 0);
			if (expected != actual) return fail("checksum mismatch"), 0;
			at += 4;
		}
		return at;
	}
};

} // namespace

Output decompress(const uint8_t *data, size_t size, size_t limit)
{
	Output result;
	Decoder d(result.bytes, limit);
	size_t at = 0;
	if (size == 0) {
		result.error = "empty input";
		return result;
	}
	while (at < size) {
		size_t used = d.frame(data + at, size - at);
		if (!d.error.empty()) {
			result.error = d.error;
			return result;
		}
		if (d.stopped) return result;
		at += used;
	}
	return result;
}

} // namespace s3::data::zstd
