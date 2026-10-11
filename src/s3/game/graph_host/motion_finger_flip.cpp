// Port of skate-game/src/graph_host/motion_finger_flip.rs (module `motion_finger_flip`).
#include "s3/game/graph_host/motion_finger_flip.hpp"

#include "s3/game/graph_host/motion_collections.hpp"

namespace s3::game::graph_host::motion_finger_flip {

rs::Result<Settings, std::string> Settings::load(const s3::data::Collections& data) {
    // GlobalAttributeCollection164 is selected at8289FD10..30. The
    // anim_motion schema places this PointNegGraphData8 field at190.
    // Preserve numeric identities because the retail vault omits names.
    S3_MOTION_TRY(const auto words, motion_collections::words<20>(data, "anim_motion", "Hash_41DB0C4F82003A15",
                                                                   "Hash_E0C1407B688858AD"));
    const float minimum = rs::from_bits(words[0]);
    const float maximum = rs::from_bits(words[2]);
    if (!rs::is_finite(minimum) || !rs::is_finite(maximum) || minimum > maximum) {
        return rs::make_err(std::string("FingerFlipOut has invalid stock timer bounds"));
    }
    return Settings{minimum, maximum, motion_collections::graph<8>(words, 4, 12)};
}

}  // namespace s3::game::graph_host::motion_finger_flip
