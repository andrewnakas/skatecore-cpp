// Shared core.pack loader for the camera tests. Native tests read
// $SKATE3_PACK or ~/Documents/haloskate-ref/cleanpack/core.pack and skip when it is missing.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "s3/data/vfs.hpp"

namespace s3test::cam {

inline std::string pack_path() {
    if (const char* env = std::getenv("SKATE3_PACK")) return env;
    if (const char* home = std::getenv("HOME")) return std::string(home) + "/Documents/haloskate-ref/cleanpack/core.pack";
    return {};
}

/// Pack paths are `assets/private/...`; the camera's asset root inside the pack.
inline const std::string ROOT = "assets";

/// Loaded once; nullptr when the pack is missing or invalid.
inline const s3::data::Vfs* pack_vfs() {
    static bool tried = false;
    static std::unique_ptr<s3::data::Vfs> vfs;
    if (!tried) {
        tried = true;
        const std::string path = pack_path();
        std::ifstream in(path, std::ios::binary);
        if (!in) return nullptr;
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        auto loaded = s3::data::Vfs::from_pack(std::move(bytes));
        if (!loaded.ok) {
            std::cout << "  pack install failed: " << loaded.error << "\n";
            return nullptr;
        }
        vfs = std::make_unique<s3::data::Vfs>(std::move(loaded.value));
    }
    return vfs.get();
}

}  // namespace s3test::cam

/// `S3_REQUIRE_PACK(vfs);` declares `const s3::data::Vfs* vfs` or returns from the test (skip).
#define S3_REQUIRE_PACK(var)                                                        \
    const s3::data::Vfs* var = ::s3test::cam::pack_vfs();                           \
    if (var == nullptr) {                                                           \
        std::cout << "SKIP " << __func__ << " (no core.pack)\n";                    \
        return;                                                                     \
    }
