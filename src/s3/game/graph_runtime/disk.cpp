// Host file adapter for the s3::data Vfs loaders (see disk.hpp).
#include "s3/game/graph_runtime/disk.hpp"

#include <fstream>
#include <iterator>

namespace s3::game::graph_runtime::disk {

void add_files(s3::data::Vfs& vfs, const std::string& root, const std::vector<std::string>& relative) {
    for (const std::string& rel : relative) {
        const std::string path = s3::data::join_path(root, rel);
        std::ifstream file(path, std::ios::binary);
        if (!file) continue;
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        vfs.add_file(path, std::move(bytes));
    }
}

s3::data::Vfs read_files(const std::string& root, const std::vector<std::string>& relative) {
    s3::data::Vfs vfs;
    add_files(vfs, root, relative);
    return vfs;
}

s3::data::Vfs read_banks(const std::string& root) { return read_files(root, {ON_BOARD_BANK, OFF_BOARD_BANK}); }

}  // namespace s3::game::graph_runtime::disk
