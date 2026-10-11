// AttribSys lookup8 identity. Numeric names preserve fields without debug strings.
// (skate-data attrib_hash.rs)
#pragma once
#include <cstdint>
#include <string>

namespace s3::data::attrib_hash {

uint64_t hash(const std::string &text);
// `Hash_XXXXXXXXXXXXXXXX` (uppercase): parses `Hash_`/`0x` hex prefixes, else hashes.
std::string numeric_name(const std::string &text);

} // namespace s3::data::attrib_hash

namespace s3::data::sha256 {
// SHA-256 source identity for private bank bytes (FIPS 180-4), lowercase hex.
std::string digest(const uint8_t *bytes, size_t size);
} // namespace s3::data::sha256
