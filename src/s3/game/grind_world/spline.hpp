// Port of skate-game/src/grind_world/spline.rs.
//! User-authorized ZIP spline adapter, checked against original TU3 82C1E568,
//! 82C1E6C0, 82C1EEF0 and 82C1F098 in default.patched.xex (431b8eba...).
//! Keep the native cubic payload; the game's contact primitive is its chord
//! D -> (A+B)+(C+D), including for retail cubic segments.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "s3/core/physics/grind_contact.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/skate_map.hpp"

namespace s3::game::grind_world::spline {

namespace rs = ::s3::rs;
using ::s3::data::Rail;
using ::s3::data::SkateMap;
using ::s3::physics::grind_contact::Primitive;

/// Native primitive side metadata; contact's endpoint/owner API stays unchanged.
struct PrimitiveMetadata {
    /// Both authored header u64s, +0 and +8. The extractor names the latter
    /// `type_signature`; preserve it verbatim, not the containing asset ID.
    std::array<std::uint64_t, 2> spline_guids{};
    std::uint32_t segment_index = 0;
    /// 82C1E568 computed near-vertical high bit; newly allocated lower bits zero.
    std::uint32_t flags = 0;
};
S3_DERIVE_EQ(PrimitiveMetadata, spline_guids, segment_index, flags)

/// `map: Option<&SkateMap>` -> nullable pointer.
rs::Result<std::vector<std::uint8_t>, std::string> build(const SkateMap* map);
rs::Result<std::vector<std::uint8_t>, std::string> build_rails(const std::vector<Rail>& rails);
rs::Result<std::vector<Primitive>, std::string> primitives(const SkateMap* map);
// Only accepts a blob returned by build/build_rails; public package input is
// validated there. Owner is a map-local header handle, not the repeating ID.
rs::Result<std::vector<Primitive>, std::string> primitives_from_blob(const std::vector<std::uint8_t>& bytes);
rs::Result<std::pair<std::vector<Primitive>, std::vector<PrimitiveMetadata>>, std::string> decoded_from_blob(
    const std::vector<std::uint8_t>& bytes);
// Private in Rust.
void put(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t w);

}  // namespace s3::game::grind_world::spline
