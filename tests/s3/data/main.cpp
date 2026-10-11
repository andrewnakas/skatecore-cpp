// Test runner for s3::data (build/s3_data_tests).
#include <chrono>
#include <cstdlib>
#include <fstream>

#include "test.hpp"

namespace s3test {

std::vector<Case> &registry()
{
	static std::vector<Case> cases;
	return cases;
}
int failures = 0;
const char *current = "";

void fail(const char *file, int line, const std::string &what)
{
	failures++;
	std::printf("  FAIL %s (%s:%d): %s\n", current, file, line, what.c_str());
}

static std::string home_path(const char *env, const char *relative)
{
	if (const char *v = std::getenv(env)) return v;
	const char *home = std::getenv("HOME");
	if (!home) return "";
	return std::string(home) + relative;
}
std::string pack_path() { return home_path("S3_PACK", "/Documents/haloskate-ref/cleanpack/core.pack"); }
std::string map_path() { return home_path("S3_MAP", "/Documents/haloskate-ref/cleanpack/KenneyPark.skate"); }

bool read_file(const std::string &path, std::vector<uint8_t> &out)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	f.seekg(0, std::ios::end);
	std::streamoff n = f.tellg();
	f.seekg(0, std::ios::beg);
	out.resize((size_t)n);
	if (n > 0) f.read((char *)out.data(), n);
	return (bool)f;
}

std::vector<uint8_t> zlib_stored(const std::vector<uint8_t> &data)
{
	std::vector<uint8_t> out = {0x78, 0x01};
	size_t at = 0;
	do {
		size_t n = data.size() - at < 65535 ? data.size() - at : 65535;
		bool last = at + n == data.size();
		out.push_back(last ? 1 : 0);
		out.push_back((uint8_t)n);
		out.push_back((uint8_t)(n >> 8));
		out.push_back((uint8_t)~n);
		out.push_back((uint8_t)(~n >> 8));
		out.insert(out.end(), data.begin() + at, data.begin() + at + n);
		at += n;
	} while (at < data.size());
	uint32_t a = 1, b = 0;
	for (uint8_t v : data) {
		a = (a + v) % 65521;
		b = (b + a) % 65521;
	}
	uint32_t adler = b << 16 | a;
	for (int i = 3; i >= 0; i--) out.push_back((uint8_t)(adler >> (8 * i)));
	return out;
}

std::vector<uint8_t> zstd_raw(const std::vector<uint8_t> &data)
{
	std::vector<uint8_t> out = {0x28, 0xB5, 0x2F, 0xFD};
	out.push_back(0xA0); // FCS 4 bytes (flag 2), single segment
	put_u32(out, (uint32_t)data.size());
	size_t at = 0;
	do {
		size_t n = data.size() - at < (128u << 10) ? data.size() - at : (128u << 10);
		bool last = at + n == data.size();
		uint32_t header = (uint32_t)(n << 3) | (last ? 1u : 0u); // block type 0 (raw)
		out.push_back((uint8_t)header);
		out.push_back((uint8_t)(header >> 8));
		out.push_back((uint8_t)(header >> 16));
		out.insert(out.end(), data.begin() + at, data.begin() + at + n);
		at += n;
	} while (at < data.size());
	return out;
}

} // namespace s3test

int main(int argc, char **argv)
{
	const char *filter = argc > 1 ? argv[1] : nullptr;
	int run = 0;
	for (auto &c : s3test::registry()) {
		if (filter && !std::strstr(c.name, filter)) continue;
		s3test::current = c.name;
		int before = s3test::failures;
		auto t0 = std::chrono::steady_clock::now();
		c.fn();
		double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		std::printf("%s %s (%.1f ms)\n", s3test::failures == before ? "ok  " : "FAIL", c.name, ms);
		run++;
	}
	std::printf("\ns3_data_tests: %d tests, %d failures\n", run, s3test::failures);
	return s3test::failures == 0 ? 0 : 1;
}
