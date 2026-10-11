// SKPK1 in-memory file tree (port of skate-vfs install_pack + wasm `mem`).
#include "vfs.hpp"

#include <cstring>

namespace s3::data {

SharedBytes SharedBytes::from_vec(std::vector<uint8_t> bytes)
{
	auto owner = std::make_shared<const std::vector<uint8_t>>(std::move(bytes));
	SharedBytes s;
	s.data = owner->data();
	s.size = owner->size();
	s.owner = std::move(owner);
	return s;
}

std::string join_path(const std::string &base, const std::string &rel)
{
	if (!rel.empty() && rel[0] == '/') return rel;
	if (base.empty()) return rel;
	if (base.back() == '/') return base + rel;
	return base + "/" + rel;
}

std::string normalize_path(const std::string &path)
{
	std::vector<std::string> parts;
	std::string part;
	auto flush = [&]() {
		if (part.empty() || part == ".") {
		} else if (part == "..") {
			if (!parts.empty()) parts.pop_back();
		} else {
			parts.push_back(part);
		}
		part.clear();
	};
	for (char c : path) {
		if (c == '/' || c == '\\') flush();
		else part += c;
	}
	flush();
	std::string out;
	for (size_t i = 0; i < parts.size(); i++) {
		if (i) out += '/';
		out += parts[i];
	}
	return out;
}

bool is_utf8(const uint8_t *s, size_t n)
{
	size_t i = 0;
	while (i < n) {
		uint8_t c = s[i];
		if (c < 0x80) {
			i++;
			continue;
		}
		size_t len;
		uint32_t cp;
		if ((c & 0xE0) == 0xC0) len = 2, cp = c & 0x1F;
		else if ((c & 0xF0) == 0xE0) len = 3, cp = c & 0x0F;
		else if ((c & 0xF8) == 0xF0) len = 4, cp = c & 0x07;
		else return false;
		if (i + len > n) return false;
		for (size_t k = 1; k < len; k++) {
			if ((s[i + k] & 0xC0) != 0x80) return false;
			cp = (cp << 6) | (s[i + k] & 0x3F);
		}
		if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000)) return false;
		if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
		i += len;
	}
	return true;
}

Result<Vfs> Vfs::from_pack(std::vector<uint8_t> pack, const std::string &root)
{
	Vfs vfs;
	S3_CHECK(vfs.install_pack(std::make_shared<const std::vector<uint8_t>>(std::move(pack)), root));
	return vfs;
}

Result<Vfs> Vfs::from_pack(const uint8_t *data, size_t size, const std::string &root)
{
	return from_pack(std::vector<uint8_t>(data, data + size), root);
}

Result<size_t> Vfs::install_pack(std::shared_ptr<const std::vector<uint8_t>> pack, const std::string &root)
{
	const uint8_t *p = pack->data();
	size_t len = pack->size(), at = 0;
	auto take = [&](size_t n, const uint8_t *&out) -> bool {
		if (n > len - at) return false;
		out = p + at;
		at += n;
		return true;
	};
	const uint8_t *s;
	if (!take(8, s)) return Error{"Truncated pack"};
	if (std::memcmp(s, "SKPK1\0\0\0", 8) != 0) return Error{"Not an SKPK1 pack"};
	if (!take(4, s)) return Error{"Truncated pack"};
	uint32_t count = (uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | (uint32_t)s[3] << 24;
	for (uint32_t i = 0; i < count; i++) {
		if (!take(4, s)) return Error{"Truncated pack"};
		uint32_t n = (uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | (uint32_t)s[3] << 24;
		if (!take(n, s)) return Error{"Truncated pack"};
		if (!is_utf8(s, n)) return Error{"Pack path is not UTF-8"};
		std::string name((const char *)s, n);
		if (!take(8, s)) return Error{"Truncated pack"};
		uint64_t size = 0;
		for (int k = 7; k >= 0; k--) size = size << 8 | s[k];
		if (size > (uint64_t)(len - at)) return Error{"Truncated pack"};
		const uint8_t *data;
		take((size_t)size, data);
		bool unsafe = !name.empty() && name[0] == '/';
		size_t b = 0;
		while (!unsafe && b <= name.size()) {
			size_t e = name.find('/', b);
			if (e == std::string::npos) e = name.size();
			if (name.compare(b, e - b, "..") == 0 && e - b == 2) unsafe = true;
			b = e + 1;
		}
		if (unsafe) return Error{"Unsafe pack path " + name};
		SharedBytes bytes;
		bytes.owner = pack;
		bytes.data = data;
		bytes.size = (size_t)size;
		files_[normalize_path(join_path(root, name))] = bytes;
	}
	return (size_t)count;
}

void Vfs::add_file(const std::string &path, std::vector<uint8_t> bytes)
{
	files_[normalize_path(path)] = SharedBytes::from_vec(std::move(bytes));
}

void Vfs::add_file(const std::string &path, SharedBytes bytes) { files_[normalize_path(path)] = std::move(bytes); }

bool Vfs::is_file(const std::string &path) const { return files_.count(normalize_path(path)) != 0; }

bool Vfs::is_dir(const std::string &path) const
{
	std::string key = normalize_path(path);
	if (key.empty()) return true;
	std::string prefix = key + "/";
	auto it = files_.lower_bound(prefix);
	return it != files_.end() && it->first.compare(0, prefix.size(), prefix) == 0;
}

Result<SharedBytes> Vfs::read(const std::string &path) const
{
	std::string key = normalize_path(path);
	auto it = files_.find(key);
	if (it == files_.end()) return Error{"/" + key + ": not found"};
	return it->second;
}

Result<std::vector<uint8_t>> Vfs::read_vec(const std::string &path) const
{
	S3_TRY(SharedBytes b, read(path));
	return b.to_vec();
}

Result<std::string> Vfs::read_to_string(const std::string &path) const
{
	S3_TRY(SharedBytes b, read(path));
	if (!is_utf8(b.data, b.size)) return Error{"stream did not contain valid UTF-8"};
	return std::string((const char *)b.data, b.size);
}

Result<std::vector<std::string>> Vfs::read_dir(const std::string &path) const
{
	std::string key = normalize_path(path);
	if (!is_dir(key)) return Error{"/" + key + ": not found"};
	std::string prefix = key.empty() ? "" : key + "/";
	// Sorted keys can interleave children ("d/a.x" < "d/a/y"), so collect into a set.
	std::map<std::string, int> unique;
	for (auto it = files_.lower_bound(prefix); it != files_.end() && it->first.compare(0, prefix.size(), prefix) == 0; ++it) {
		std::string rest = it->first.substr(prefix.size());
		size_t slash = rest.find('/');
		unique[slash == std::string::npos ? rest : rest.substr(0, slash)] = 1;
	}
	std::vector<std::string> out;
	for (auto &kv : unique) out.push_back(kv.first);
	return out;
}

std::vector<std::string> Vfs::files_under(const std::string &dir) const
{
	std::string key = normalize_path(dir);
	std::string prefix = key.empty() ? "" : key + "/";
	std::vector<std::string> out;
	for (auto it = files_.lower_bound(prefix); it != files_.end() && it->first.compare(0, prefix.size(), prefix) == 0; ++it)
		out.push_back(it->first);
	return out;
}

} // namespace s3::data
