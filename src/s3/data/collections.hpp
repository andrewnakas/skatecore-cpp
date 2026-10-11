// Typed access to converted stock skater XML values, without engine objects.
// (skate-data collections.rs: VLT/AttribSys `private/stock/skater-collections.json`)
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "result.hpp"
#include "vfs.hpp"

namespace s3::data {

struct FieldArray
{
	uint32_t capacity = 0;
	uint32_t element_size = 0;
	uint32_t alignment = 0;
	std::vector<std::string> items;
};

struct Field
{
	std::string type_name; // json "type"
	std::string data;
	/// Array attributes (`vlt.py` `array_items`): `data` holds only the header lanes, the
	/// elements are here, one hex string per item.
	std::optional<FieldArray> array;
};

struct Collection
{
	std::string class_name; // json "class"
	std::string key;
	std::string parent;
	std::map<std::string, Field> fields;
	std::string source;
	std::string sha256;
	// Cached attrib_hash::numeric_name of class_name/key (lookup acceleration only).
	std::string class_id;
	std::string key_id;
};

// Rust `decode_words::<N>`: N big-endian hex words, whitespace ignored.
Status decode_words(const std::string &text, uint32_t *out, size_t n);

class Collections
{
public:
	uint32_t version = 0;
	std::vector<Collection> collections;

	const std::vector<Collection> &entries() const { return collections; }

	// Load `<asset_root>/private/stock/skater-collections.json` with version/identity checks.
	static Result<Collections> load(const Vfs &vfs, const std::string &asset_root);
	// serde_json::from_slice only (no version/duplicate validation), as the Rust tests use.
	static Result<Collections> from_json(std::string_view text);

	Result<const Field *> field(const std::string &cls, const std::string &key, const std::string &name) const;
	Result<float> float_(const std::string &cls, const std::string &key, const std::string &name) const;
	Result<float> float_value(const std::string &cls, const std::string &key, const std::string &name) const
	{
		return float_(cls, key, name);
	}
	Result<uint32_t> integer(const std::string &cls, const std::string &key, const std::string &name) const;
	Result<bool> boolean(const std::string &cls, const std::string &key, const std::string &name) const;
	/// Every element of a float array attribute, in order (the index AttribSys
	/// `sub_82B72420` takes as its third argument).
	Result<std::vector<float>> float_items(const std::string &cls, const std::string &key, const std::string &name) const;
	template <size_t N>
	Result<std::array<uint32_t, N>> words(const std::string &cls, const std::string &key, const std::string &name) const
	{
		S3_TRY(const Field *f, field(cls, key, name));
		std::array<uint32_t, N> out{};
		S3_CHECK(decode_words(f->data, out.data(), N));
		return out;
	}
};

} // namespace s3::data
