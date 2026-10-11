// In-memory file tree over an SKPK1 pack (skate-vfs `install_pack` + the wasm `mem` tree).
// Format: magic `SKPK1\0\0\0`, u32 count, then per entry u32 path_len, utf8 path
// (relative, forward slashes), u64 data_len, bytes (all little endian).
// Entries are views into the shared pack buffer (no per-file copy); add_file() adds more
// (the web demo inserts `maps/<name>.skate`). Paths are normalized like skate-vfs `norm`:
// `\` -> `/`, empty and `.` components dropped, `..` pops. Keys carry no leading slash.
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "result.hpp"

namespace s3::data {

// Bytes kept alive by `owner` (the pack buffer or an added file).
struct SharedBytes
{
	std::shared_ptr<const std::vector<uint8_t>> owner;
	const uint8_t *data = nullptr;
	size_t size = 0;
	const uint8_t *begin() const { return data; }
	const uint8_t *end() const { return data + size; }
	std::vector<uint8_t> to_vec() const { return std::vector<uint8_t>(data, data + size); }
	static SharedBytes from_vec(std::vector<uint8_t> bytes);
};

// Rust Path::join for '/'-separated strings (absolute `rel` replaces `base`).
std::string join_path(const std::string &base, const std::string &rel);
// skate-vfs `norm` without the leading slash.
std::string normalize_path(const std::string &path);

class Vfs
{
public:
	Vfs() = default;
	// Parse a pack and install every entry below `root` (empty: pack paths as is).
	static Result<Vfs> from_pack(std::vector<uint8_t> pack, const std::string &root = "");
	static Result<Vfs> from_pack(const uint8_t *data, size_t size, const std::string &root = "");
	// skate_vfs::install_pack: returns the entry count.
	Result<size_t> install_pack(std::shared_ptr<const std::vector<uint8_t>> pack, const std::string &root = "");
	// skate_vfs::insert.
	void add_file(const std::string &path, std::vector<uint8_t> bytes);
	void add_file(const std::string &path, SharedBytes bytes);

	bool is_file(const std::string &path) const;
	bool is_dir(const std::string &path) const;
	// skate_vfs::read; the error text is "<norm path>: not found" like the wasm tree.
	Result<SharedBytes> read(const std::string &path) const;
	Result<std::vector<uint8_t>> read_vec(const std::string &path) const;
	// skate_vfs::read_to_string (UTF-8 checked).
	Result<std::string> read_to_string(const std::string &path) const;
	// Direct children names of a directory (sorted), like read_dir.
	Result<std::vector<std::string>> read_dir(const std::string &path) const;
	// Every file path (normalized) below `dir`, recursively, sorted.
	std::vector<std::string> files_under(const std::string &dir) const;
	size_t file_count() const { return files_.size(); }

private:
	std::map<std::string, SharedBytes> files_;
};

bool is_utf8(const uint8_t *data, size_t size);

} // namespace s3::data
