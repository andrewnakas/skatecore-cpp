// Minimal test registry for the s3::data tests (ported Rust #[test]s + pack integration).
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace s3test {

struct Case
{
	const char *name;
	void (*fn)();
};
std::vector<Case> &registry();
struct Reg
{
	Reg(const char *name, void (*fn)()) { registry().push_back({name, fn}); }
};
extern int failures;
extern const char *current;
void fail(const char *file, int line, const std::string &what);

// Paths of the native test data (env S3_PACK / S3_MAP override); empty when absent.
std::string pack_path();
std::string map_path();
bool read_file(const std::string &path, std::vector<uint8_t> &out);

// Little-endian writers used by fixtures.
inline void put_u32(std::vector<uint8_t> &b, uint32_t v)
{
	for (int i = 0; i < 4; i++) b.push_back((uint8_t)(v >> (8 * i)));
}
inline void put_f32(std::vector<uint8_t> &b, float f)
{
	uint32_t v;
	std::memcpy(&v, &f, 4);
	put_u32(b, v);
}
// A valid zlib stream using stored DEFLATE blocks (flate2 stand-in for fixtures).
std::vector<uint8_t> zlib_stored(const std::vector<uint8_t> &data);
// A valid zstd frame using raw blocks (zstd::encode_all stand-in for fixtures).
std::vector<uint8_t> zstd_raw(const std::vector<uint8_t> &data);

} // namespace s3test

#define S3_TEST(name)                                         \
	static void name();                                       \
	static ::s3test::Reg s3test_reg_##name(#name, &name);     \
	static void name()

#define CHECK(cond)                                                              \
	do {                                                                         \
		if (!(cond)) {                                                           \
			::s3test::fail(__FILE__, __LINE__, "CHECK(" #cond ")");              \
			return;                                                              \
		}                                                                        \
	} while (0)

#define CHECK_MSG(cond, msg)                                                     \
	do {                                                                         \
		if (!(cond)) {                                                           \
			::s3test::fail(__FILE__, __LINE__, std::string("CHECK(" #cond "): ") + (msg)); \
			return;                                                              \
		}                                                                        \
	} while (0)

#define CHECK_EQ(a, b)                                                                                   \
	do {                                                                                                 \
		auto s3_a = (a);                                                                                 \
		auto s3_b = (b);                                                                                 \
		if (!(s3_a == s3_b)) {                                                                           \
			::s3test::fail(__FILE__, __LINE__, "CHECK_EQ(" #a ", " #b ")");                              \
			return;                                                                                      \
		}                                                                                                \
	} while (0)

// Result must be ok (prints its error otherwise).
#define CHECK_OK(r)                                                                          \
	do {                                                                                     \
		if (!(r).ok) {                                                                       \
			::s3test::fail(__FILE__, __LINE__, std::string("CHECK_OK(" #r "): ") + (r).error); \
			return;                                                                          \
		}                                                                                    \
	} while (0)

#define CHECK_ERR_CONTAINS(r, text)                                                                      \
	do {                                                                                                 \
		auto s3_r = (r);                                                                                 \
		if (s3_r.ok || s3_r.error.find(text) == std::string::npos) {                                     \
			::s3test::fail(__FILE__, __LINE__,                                                           \
			               std::string("CHECK_ERR_CONTAINS(" #r ", " #text "): ") + (s3_r.ok ? "ok" : s3_r.error)); \
			return;                                                                                      \
		}                                                                                                \
	} while (0)
