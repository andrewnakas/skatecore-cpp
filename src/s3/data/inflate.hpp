// zlib (RFC 1950) / DEFLATE (RFC 1951) decoder for SKATE storage method 1 and
// methods 7..10 (replaces flate2's ZlibDecoder). No dependencies.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace s3::data::inflate {

struct Output
{
	std::vector<uint8_t> bytes;
	size_t total_in = 0;    // zlib bytes consumed (header + deflate + adler32 when complete)
	bool complete = false;  // stream end and checksum reached
	std::string error;      // non-empty: corrupt stream
};

// Decodes up to `limit` output bytes (like `Read::take(limit).read_to_end`). Stops early
// without error when the limit is reached before the stream ends (complete=false).
Output zlib_decompress(const uint8_t *data, size_t size, size_t limit);

} // namespace s3::data::inflate
