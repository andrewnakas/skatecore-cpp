// zlib/DEFLATE decoder (canonical Huffman with a 10-bit fast table).
#include "inflate.hpp"

#include <cstring>

namespace s3::data::inflate {

namespace {

constexpr int MAXBITS = 15;
constexpr int FASTBITS = 10;

struct Huffman
{
	uint16_t count[MAXBITS + 1];
	uint16_t symbol[320];
	// fast[code] = (symbol << 4) | length, 0 when the code is longer than FASTBITS.
	uint16_t fast[1 << FASTBITS];
};

struct State
{
	const uint8_t *in;
	size_t size;
	size_t pos = 0;
	uint64_t bitbuf = 0;
	int bitcnt = 0;
	// Output: `out` is the buffer (its size is the capacity), `len` the bytes produced.
	std::vector<uint8_t> &out;
	size_t len = 0;
	size_t limit;

	State(const uint8_t *d, size_t n, std::vector<uint8_t> &o, size_t l) : in(d), size(n), out(o), limit(l) {}

	bool need(int n)
	{
		while (bitcnt < n) {
			if (pos >= size) return false;
			bitbuf |= (uint64_t)in[pos++] << bitcnt;
			bitcnt += 8;
		}
		return true;
	}
	// Prefetch whole bytes (align() gives unused ones back).
	void refill()
	{
		while (bitcnt <= 56 && pos < size) {
			bitbuf |= (uint64_t)in[pos++] << bitcnt;
			bitcnt += 8;
		}
	}
	// Drop the partial byte; give back whole prefetched bytes.
	void align()
	{
		pos -= (size_t)(bitcnt / 8);
		bitbuf = 0;
		bitcnt = 0;
	}
	bool bits(int n, uint32_t &v)
	{
		if (!need(n)) return false;
		v = (uint32_t)(bitbuf & ((1ull << n) - 1));
		bitbuf >>= n;
		bitcnt -= n;
		return true;
	}
	// Room for `n` more bytes (bounded by the limit); returns how many fit.
	size_t reserve(size_t n)
	{
		size_t room = limit - len;
		if (n > room) n = room;
		if (len + n > out.size()) {
			size_t grow = out.size() * 2 + (64u << 10);
			size_t want = len + n > grow ? len + n : grow;
			if (want > limit) want = limit;
			out.resize(want);
		}
		return n;
	}
};

// Returns 0 when complete, >0 for an incomplete set, <0 for an over-subscribed set.
int construct(Huffman &h, const uint16_t *length, int n)
{
	std::memset(h.count, 0, sizeof h.count);
	std::memset(h.fast, 0, sizeof h.fast);
	for (int s = 0; s < n; s++) h.count[length[s]]++;
	if (h.count[0] == n) return 0; // no codes: complete but decoding will fail
	int left = 1;
	for (int len = 1; len <= MAXBITS; len++) {
		left <<= 1;
		left -= h.count[len];
		if (left < 0) return -1;
	}
	uint16_t offs[MAXBITS + 1];
	offs[1] = 0;
	for (int len = 1; len < MAXBITS; len++) offs[len + 1] = (uint16_t)(offs[len] + h.count[len]);
	for (int s = 0; s < n; s++)
		if (length[s] != 0) h.symbol[offs[length[s]]++] = (uint16_t)s;
	// Fast table: canonical codes assigned in (length, symbol) order, stored bit-reversed.
	int code = 0, index = 0;
	for (int len = 1; len <= MAXBITS; len++) {
		for (int k = 0; k < h.count[len]; k++, index++, code++) {
			if (len > FASTBITS) continue;
			int rev = 0;
			for (int b = 0; b < len; b++) rev |= ((code >> b) & 1) << (len - 1 - b);
			for (int fill = rev; fill < (1 << FASTBITS); fill += 1 << len) h.fast[fill] = (uint16_t)(h.symbol[index] << 4 | len);
		}
		code <<= 1;
	}
	return left;
}

// Decode one symbol; -1 on out of input, -2 on invalid code.
inline int decode(State &s, const Huffman &h)
{
	if (s.bitcnt < MAXBITS) s.refill(); // may stay short near the end; fall back below
	uint16_t e = h.fast[s.bitbuf & ((1u << FASTBITS) - 1)];
	if (e && (e & 15) <= s.bitcnt) {
		int len = e & 15;
		s.bitbuf >>= len;
		s.bitcnt -= len;
		return e >> 4;
	}
	// Slow canonical decode, bit by bit (codes longer than FASTBITS, or the stream end).
	int code = 0, first = 0, index = 0;
	for (int len = 1; len <= MAXBITS; len++) {
		uint32_t bit;
		if (!s.bits(1, bit)) return -1;
		code |= (int)bit;
		int count = h.count[len];
		if (code - count < first) return h.symbol[index + (code - first)];
		index += count;
		first += count;
		first <<= 1;
		code <<= 1;
	}
	return -2;
}

const uint16_t LBASE[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const uint16_t LEXT[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const uint16_t DBASE[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
                            1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const uint16_t DEXT[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

// 0 ok, 1 limit reached, <0 error
int codes(State &s, const Huffman &lencode, const Huffman &distcode)
{
	for (;;) {
		int symbol = decode(s, lencode);
		if (symbol < 0) return symbol == -1 ? -10 : -11;
		if (symbol < 256) {
			if (s.reserve(1) == 0) return 1;
			s.out[s.len++] = (uint8_t)symbol;
		} else if (symbol == 256) {
			return 0;
		} else {
			symbol -= 257;
			if (symbol >= 29) return -12;
			uint32_t extra;
			if (!s.bits(LEXT[symbol], extra)) return -10;
			size_t len = LBASE[symbol] + extra;
			int dsym = decode(s, distcode);
			if (dsym < 0) return dsym == -1 ? -10 : -11;
			if (dsym >= 30) return -12;
			if (!s.bits(DEXT[dsym], extra)) return -10;
			size_t dist = DBASE[dsym] + extra;
			if (dist > s.len) return -13;
			size_t n = s.reserve(len);
			uint8_t *o = s.out.data() + s.len;
			for (size_t k = 0; k < n; k++) o[k] = o[k - dist]; // overlapping copy in byte order
			s.len += n;
			if (n < len) return 1;
		}
	}
}

int fixed_block(State &s)
{
	static Huffman lencode, distcode;
	static bool built = false;
	if (!built) {
		uint16_t lengths[320];
		int sym = 0;
		for (; sym < 144; sym++) lengths[sym] = 8;
		for (; sym < 256; sym++) lengths[sym] = 9;
		for (; sym < 280; sym++) lengths[sym] = 7;
		for (; sym < 288; sym++) lengths[sym] = 8;
		construct(lencode, lengths, 288);
		for (sym = 0; sym < 30; sym++) lengths[sym] = 5;
		construct(distcode, lengths, 30);
		built = true;
	}
	return codes(s, lencode, distcode);
}

int dynamic_block(State &s)
{
	static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
	uint32_t nlen, ndist, ncode;
	if (!s.bits(5, nlen) || !s.bits(5, ndist) || !s.bits(4, ncode)) return -10;
	nlen += 257;
	ndist += 1;
	ncode += 4;
	if (nlen > 286 || ndist > 30) return -3;
	uint16_t lengths[320] = {0};
	uint32_t index;
	for (index = 0; index < ncode; index++) {
		uint32_t v;
		if (!s.bits(3, v)) return -10;
		lengths[order[index]] = (uint16_t)v;
	}
	Huffman lencode, distcode;
	if (construct(lencode, lengths, 19) != 0) return -4;
	index = 0;
	while (index < nlen + ndist) {
		int symbol = decode(s, lencode);
		if (symbol < 0) return symbol == -1 ? -10 : -11;
		if (symbol < 16) {
			lengths[index++] = (uint16_t)symbol;
		} else {
			uint16_t len = 0;
			uint32_t rep;
			if (symbol == 16) {
				if (index == 0) return -5;
				len = lengths[index - 1];
				if (!s.bits(2, rep)) return -10;
				rep += 3;
			} else if (symbol == 17) {
				if (!s.bits(3, rep)) return -10;
				rep += 3;
			} else {
				if (!s.bits(7, rep)) return -10;
				rep += 11;
			}
			if (index + rep > nlen + ndist) return -6;
			while (rep--) lengths[index++] = len;
		}
	}
	if (lengths[256] == 0) return -9;
	int err = construct(lencode, lengths, (int)nlen);
	if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1)) return -7;
	err = construct(distcode, lengths + nlen, (int)ndist);
	if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1)) return -8;
	return codes(s, lencode, distcode);
}

int stored_block(State &s)
{
	s.align();
	if (s.pos + 4 > s.size) return -10;
	uint32_t len = s.in[s.pos] | (uint32_t)s.in[s.pos + 1] << 8;
	uint32_t nlen = s.in[s.pos + 2] | (uint32_t)s.in[s.pos + 3] << 8;
	s.pos += 4;
	if (len != (~nlen & 0xffff)) return -2;
	size_t avail = s.size - s.pos;
	size_t n = s.reserve(len < avail ? len : avail);
	if (n) std::memcpy(s.out.data() + s.len, s.in + s.pos, n);
	s.len += n;
	s.pos += n;
	if (n < len) return n < avail ? 1 : -10;
	return 0;
}

} // namespace

Output zlib_decompress(const uint8_t *data, size_t size, size_t limit)
{
	Output result;
	if (size < 2) {
		result.error = size == 0 ? "unexpected end of file" : "corrupt deflate stream";
		return result;
	}
	uint8_t cmf = data[0], flg = data[1];
	if ((cmf & 15) != 8 || (cmf >> 4) > 7 || ((uint32_t)cmf << 8 | flg) % 31 != 0 || (flg & 0x20)) {
		result.error = "corrupt deflate stream";
		return result;
	}
	// Start with min(limit, 8 MiB) like the Rust reader's reservation; grow on demand.
	result.bytes.resize(limit < ((size_t)8 << 20) ? limit : ((size_t)8 << 20));
	State s(data, size, result.bytes, limit);
	s.pos = 2;
	auto finish = [&](const char *error) {
		result.bytes.resize(s.len);
		if (error) result.error = error;
		result.total_in = s.pos;
		return result;
	};
	int last;
	do {
		uint32_t v, type;
		if (!s.bits(1, v) || !s.bits(2, type)) return finish("corrupt deflate stream");
		last = (int)v;
		int err = type == 0 ? stored_block(s) : type == 1 ? fixed_block(s) : type == 2 ? dynamic_block(s) : -1;
		if (err == 1) {
			s.align();
			return finish(nullptr); // output limit reached
		}
		if (err != 0) return finish("corrupt deflate stream");
	} while (!last);
	// Byte-align and read the big-endian Adler-32 trailer.
	s.align();
	if (s.pos + 4 > size) return finish("corrupt deflate stream");
	uint32_t expected = (uint32_t)data[s.pos] << 24 | (uint32_t)data[s.pos + 1] << 16 | (uint32_t)data[s.pos + 2] << 8 | data[s.pos + 3];
	s.pos += 4;
	uint32_t a = 1, b = 0;
	const uint8_t *o = result.bytes.data();
	size_t i = 0;
	while (i < s.len) {
		size_t chunk = s.len - i < 5552 ? s.len - i : 5552;
		for (size_t k = 0; k < chunk; k++) {
			a += o[i + k];
			b += a;
		}
		a %= 65521;
		b %= 65521;
		i += chunk;
	}
	if ((b << 16 | a) != expected) return finish("corrupt deflate stream");
	result.complete = true;
	return finish(nullptr);
}

} // namespace s3::data::inflate
