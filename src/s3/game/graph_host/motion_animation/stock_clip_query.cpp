// Port of skate-game/src/graph_host/motion_animation/stock_clip_query.rs.
#include "s3/game/graph_host/motion_animation/stock_clip_query.hpp"

#include "s3/core/animation/playback_clip.hpp"
#include "s3/data/result.hpp"

namespace s3::game::graph_host::motion_animation {

namespace {
/// `Path::new(path).file_stem().and_then(to_str).unwrap_or("")`.
std::string file_stem(std::string path) {
    while (path.size() > 1 && path.back() == '/') path.pop_back();
    const std::size_t slash = path.rfind('/');
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    if (name.empty() || name == "..") return "";
    if (name == ".") return "";
    const std::size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) return name;
    return name.substr(0, dot);
}
}  // namespace

rs::Result<float, std::string> MotionAnimation::stock_clip_translation_z(std::string_view database,
                                                                         std::string_view animation) const {
    //82BA7FA4/7FDC: missing bank/clip/attribute leaves the scalar zero.
    const s3::data::BankSource* source = metadata_.source_for(std::string(animation));
    if (!source) return 0.0f;
    const std::string bank = file_stem(source->source_bank);
    if (!s3::data::eq_ignore_ascii_case(bank, std::string(database))) return 0.0f;
    auto clip = metadata_.clip(std::string(animation));
    if (!clip.ok) return rs::make_err(clip.error);
    const s3::data::ClipAttribute* attribute = nullptr;
    for (const auto& a : clip.value->attributes) {
        if (s3::data::eq_ignore_ascii_case(a.name, "AnimTransZ")) {
            attribute = &a;
            break;
        }
    }
    if (!attribute) return 0.0f;
    //82D16680 requests Attribute::Init82D164F8 at time0, irrespective of
    //the event's authored time range. Curve frame is therefore zero.
    switch (attribute->type_id) {
        case 0:
        case 1:
        case 3:
            if (attribute->payload_words.empty()) {
                return rs::make_err(std::string(database) + "/" + std::string(animation) + ": truncated AnimTransZ");
            }
            return rs::from_bits(attribute->payload_words.front());
        case 2: return s3::animation::playback_clip::sample_curve(attribute->payload_words, 0.0f);
        default: return 0.0f;  //82D1659C: no payload write for other kinds.
    }
}

}  // namespace s3::game::graph_host::motion_animation
