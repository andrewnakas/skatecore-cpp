// Host file adapter (no Rust counterpart): the Rust loaders take `&Path` and read with
// std::fs / skate_vfs, while the C++ s3::data loaders read from an s3::data::Vfs. These helpers
// load just the listed files from the filesystem into a Vfs keyed by the same joined path, so
// `Loader::load(vfs, root)` sees exactly what the Rust `Loader::load(root)` reads.
#pragma once
#include <string>
#include <vector>

#include "s3/data/vfs.hpp"

namespace s3::game::graph_runtime::disk {

/// Asset files read by the stock loaders, relative to the asset root.
inline const char* const COLLECTIONS = "private/stock/skater-collections.json";
inline const char* const MANIFEST = "private/game.json";
inline const char* const ON_BOARD_BANK = "private/stock/data/anim/OnBoard.abin";
inline const char* const OFF_BOARD_BANK = "private/stock/data/anim/OffBoard.abin";
inline const char* const AUTHORED_CLIPS = "private/custom/crouch-treflip.json";

/// Reads `join_path(root, relative)` for every listed file into `vfs`. Files that do not
/// exist are skipped: the s3::data loader then reports them as missing.
void add_files(s3::data::Vfs& vfs, const std::string& root, const std::vector<std::string>& relative);
s3::data::Vfs read_files(const std::string& root, const std::vector<std::string>& relative);
/// Both stock animation banks (`AnimationBanks::load`).
s3::data::Vfs read_banks(const std::string& root);

}  // namespace s3::game::graph_runtime::disk
