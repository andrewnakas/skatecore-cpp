// Port of simref sim/src/camera/stock_names.rs.
//! Host decoding of the collection-name hash stored in stock RefSpec values.
//! Same lookup8 byte format as the existing private XML extraction tools.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace s3::game::camera::stock_names {

std::uint64_t lookup8(const std::uint8_t* bytes, std::size_t size);
inline std::uint64_t lookup8(const std::string& text) {
    return lookup8(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}

}  // namespace s3::game::camera::stock_names
