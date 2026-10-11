// Asset access for the skate-game tests: Rust tests that read `asset_root` (the unpacked pack)
// read the cleanroom pack instead: ~/Documents/haloskate-ref/cleanpack/core.pack (override with
// S3_PACK) mounted at "" so the asset root is "assets"; KenneyPark.skate from the same dir
// (override with S3_MAP). Tests return early (skip) when the pack is missing, like the
// Rust `#[ignore]`-less private-asset tests that early-return without assets.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "s3/data/vfs.hpp"

namespace s3test::game {

inline std::string home_path(const char* env, const char* rel) {
    if (const char* p = std::getenv(env)) return p;
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + rel : std::string();
}
inline std::string pack_path() { return home_path("S3_PACK", "/Documents/haloskate-ref/cleanpack/core.pack"); }
inline std::string map_path() { return home_path("S3_MAP", "/Documents/haloskate-ref/cleanpack/KenneyPark.skate"); }
/// The asset root inside the pack VFS (Rust `asset_root`).
inline const std::string ASSET_ROOT = "assets";

inline bool read_file(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

/// Shared pack VFS (nullptr when the pack is missing). The map is added at "maps/KenneyPark.skate".
inline const s3::data::Vfs* assets() {
    static std::unique_ptr<s3::data::Vfs> vfs;
    static bool tried = false;
    if (!tried) {
        tried = true;
        std::vector<std::uint8_t> bytes;
        if (!read_file(pack_path(), bytes)) {
            std::printf("  (skip: no pack at %s)\n", pack_path().c_str());
            return nullptr;
        }
        auto v = s3::data::Vfs::from_pack(std::move(bytes));
        if (!v.ok) {
            std::printf("  (skip: pack failed: %s)\n", v.error.c_str());
            return nullptr;
        }
        vfs = std::make_unique<s3::data::Vfs>(std::move(v.value));
        std::vector<std::uint8_t> map;
        if (read_file(map_path(), map)) vfs->add_file("maps/KenneyPark.skate", std::move(map));
    }
    return vfs.get();
}

}  // namespace s3test::game

namespace s3test::game {
/// Rust `#[ignore]` private-asset tests whose expectations the current Rust simulation (and therefore this
/// bit-identical port, see docs/PARITY.md) no longer meets. They are skipped unless S3_RUN_STALE is set.
inline bool skip_stale(const char* why) {
    if (std::getenv("S3_RUN_STALE") != nullptr) return false;
    std::printf("  (skip stale #[ignore] test: %s; set S3_RUN_STALE=1 to run)\n", why);
    return true;
}
}  // namespace s3test::game
