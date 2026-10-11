// Lossless stock physics-bone records. Core consumes typed values derived here.
// (skate-data physics_skeleton.rs: `private/stock/physics-skeletons.json`)
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "result.hpp"
#include "vfs.hpp"

namespace s3::data {

struct PhysicsBone
{
	std::string name;
	uint64_t source_offset = 0;
	std::array<uint32_t, 28> words{};

	std::array<float, 3> size() const;
	std::array<float, 4> rotation() const;
	std::array<float, 3> translation() const;
};

struct PhysicsSkeleton
{
	std::string name;
	uint64_t source_offset = 0;
	std::vector<PhysicsBone> bones;

	// `path` is a VFS path, e.g. join_path(asset_root, "private/stock/physics-skeletons.json").
	static Result<PhysicsSkeleton> load(const Vfs &vfs, const std::string &path, const std::string &bank_sha256, const std::string &name);
	// The same validation on in-memory JSON text.
	static Result<PhysicsSkeleton> parse(std::string_view json, const std::string &bank_sha256, const std::string &name);
};

} // namespace s3::data
