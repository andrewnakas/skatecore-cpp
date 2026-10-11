// Asset root for tests that the Rust code marks #[ignore = "requires ... artifacts"].
// Rust reads $SKATE3_ASSET_ROOT (the unpacked `assets/` directory); we fall back to the local
// cleanpack unpack and skip (return early, printing SKIP) when neither exists.
#pragma once
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/stat.h>

namespace s3test {
inline std::string asset_root() {
    auto is_dir = [](const std::string& p) {
        struct stat st {};
        return !p.empty() && stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    };
    if (const char* env = std::getenv("SKATE3_ASSET_ROOT")) {
        if (is_dir(env)) return env;
    }
    if (const char* home = std::getenv("HOME")) {
        std::string p = std::string(home) + "/Documents/haloskate-ref/cleanpack/root/assets";
        if (is_dir(p)) return p;
    }
    return {};
}
}  // namespace s3test

/// `S3_REQUIRE_ASSETS(root);` declares `std::string root` or returns from the test (skip).
#define S3_REQUIRE_ASSETS(var)                                                   \
    const std::string var = ::s3test::asset_root();                              \
    if (var.empty()) {                                                           \
        std::cout << "SKIP " << __func__ << " (no SKATE3_ASSET_ROOT)\n";         \
        return;                                                                  \
    }
