// AttribSys lookup8 identity (attrib_hash.rs) and SHA-256 (sha256.rs).
#include "attrib_hash.hpp"

#include <cstdio>
#include <cstring>

#include "result.hpp"

namespace s3::data::attrib_hash {

static void mix(uint64_t &a, uint64_t &b, uint64_t &c)
{
	static const int shifts[4][3] = {{43, 9, 8}, {38, 23, 5}, {35, 49, 11}, {12, 18, 22}};
	for (auto &s : shifts) {
		a = (a - b - c) ^ (c >> s[0]);
		b = (b - c - a) ^ (a << s[1]);
		c = (c - a - b) ^ (b >> s[2]);
	}
}

static uint64_t le64(const unsigned char *p)
{
	uint64_t v = 0;
	for (int i = 7; i >= 0; i--) v = v << 8 | p[i];
	return v;
}

uint64_t hash(const std::string &text)
{
	if (text.empty()) return 0;
	uint64_t a = 0xabcdef0011223344ull;
	uint64_t b = a;
	uint64_t c = 0x9e3779b97f4a7c13ull;
	const unsigned char *p = (const unsigned char *)text.data();
	size_t n = text.size(), full = n / 24 * 24;
	for (size_t at = 0; at < full; at += 24) {
		a += le64(p + at);
		b += le64(p + at + 8);
		c += le64(p + at + 16);
		mix(a, b, c);
	}
	c += (uint64_t)n;
	for (size_t i = 0; i < n - full; i++) {
		uint64_t byte = p[full + i];
		if (i <= 7) a += byte << (i * 8);
		else if (i <= 15) b += byte << ((i - 8) * 8);
		else c += byte << ((i - 15) * 8);
	}
	mix(a, b, c);
	return c;
}

std::string numeric_name(const std::string &text)
{
	uint64_t key;
	bool parsed = false;
	if (text.compare(0, 5, "Hash_") == 0) parsed = parse_u64_radix(text.substr(5), 16, key);
	else if (text.compare(0, 2, "0x") == 0) parsed = parse_u64_radix(text.substr(2), 16, key);
	if (!parsed) key = hash(text);
	char out[32];
	std::snprintf(out, sizeof out, "Hash_%016llX", (unsigned long long)key);
	return out;
}

} // namespace s3::data::attrib_hash

namespace s3::data::sha256 {

static inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void compress(uint32_t state[8], const uint8_t *block)
{
	static const uint32_t K[64] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
	};
	uint32_t w[64];
	for (int i = 0; i < 16; i++)
		w[i] = (uint32_t)block[4 * i] << 24 | (uint32_t)block[4 * i + 1] << 16 | (uint32_t)block[4 * i + 2] << 8 | block[4 * i + 3];
	for (int i = 16; i < 64; i++) {
		uint32_t x = w[i - 15], y = w[i - 2];
		uint32_t s0 = rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
		uint32_t s1 = rotr(y, 17) ^ rotr(y, 19) ^ (y >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5], g = state[6], h = state[7];
	for (int i = 0; i < 64; i++) {
		uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
		uint32_t choice = (e & f) ^ (~e & g);
		uint32_t t1 = h + s1 + choice + K[i] + w[i];
		uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
		uint32_t t2 = s0 + ((a & b) ^ (a & c) ^ (b & c));
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}
	uint32_t v[8] = {a, b, c, d, e, f, g, h};
	for (int i = 0; i < 8; i++) state[i] += v[i];
}

std::string digest(const uint8_t *bytes, size_t size)
{
	uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	size_t full = size / 64 * 64;
	for (size_t at = 0; at < full; at += 64) compress(state, bytes + at);
	size_t rem = size - full;
	uint8_t tail[128] = {0};
	if (rem) std::memcpy(tail, bytes + full, rem);
	tail[rem] = 0x80;
	size_t end = rem < 56 ? 64 : 128;
	uint64_t bits = (uint64_t)size * 8;
	for (int i = 0; i < 8; i++) tail[end - 8 + i] = (uint8_t)(bits >> (56 - 8 * i));
	for (size_t at = 0; at < end; at += 64) compress(state, tail + at);
	std::string out;
	char b[9];
	for (uint32_t v : state) {
		std::snprintf(b, sizeof b, "%08x", v);
		out += b;
	}
	return out;
}

} // namespace s3::data::sha256
