// Port of skate-game/src/graph_host/motion_grind.rs (module `motion_grind`).
#include "s3/game/graph_host/motion_grind.hpp"

namespace s3::game::graph_host::motion_grind {

using s3::animation::skeleton_input::name::encode;

rs::Result<std::optional<Operation>, std::string> Operation::parse(
    const s3::data::state_graph_attributes::Attributes& a) {
    const std::string* name = a.text("name");
    if (name == nullptr) return std::optional<Operation>();
    if (*name == "CreateGrindAttributes") return std::optional<Operation>(Attributes{});
    if (*name == "ControlGrindCrouch") {
        const std::string* height = a.text("driveDistToCom");
        return std::optional<Operation>(Crouch{encode(height ? *height : std::string("disttocog"))});
    }
    if (*name == "GrindControlFade") {
        const char* required[] = {"distBoardToCogAnimAttribute", "twistAnimAttribute", "twistMGIntent"};
        const std::string* values[3] = {};
        for (int i = 0; i < 3; ++i) {
            values[i] = a.text(required[i]);
            if (values[i] == nullptr) return rs::make_err(std::string("GrindControlFade requires ") + required[i]);
        }
        return std::optional<Operation>(Fade{encode(*values[0]), encode(*values[1]), *values[2]});
    }
    return std::optional<Operation>();
}

}  // namespace s3::game::graph_host::motion_grind
