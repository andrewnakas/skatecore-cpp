// Zstandard (RFC 8878) frame decoder for SKATE storage methods 2..6 (replaces the Rust
// `zstd`/`ruzstd` crates). Supports raw/RLE/compressed blocks, Huffman literals, FSE
// sequences, repeat modes, skippable and concatenated frames and the XXH64 content
// checksum. Dictionaries are not supported (the SKATE exporter never uses them).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace s3::data::zstd {

struct Output
{
	std::vector<uint8_t> bytes;
	std::string error; // non-empty on corruption / unsupported input
};

// Decodes all frames in `data`, stopping (without error) once `limit` output bytes exist
// (like `Read::take(limit).read_to_end`).
Output decompress(const uint8_t *data, size_t size, size_t limit);

uint64_t xxh64(const uint8_t *data, size_t size, uint64_t seed);

} // namespace s3::data::zstd
