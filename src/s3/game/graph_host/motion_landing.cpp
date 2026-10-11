// Port of skate-game/src/graph_host/motion_landing.rs (module `motion_landing`).
#include "s3/game/graph_host/motion_landing.hpp"

namespace s3::game::graph_host::motion_landing {

std::optional<Operation> Operation::parse(const Attributes& a) {
    const std::string* name = a.text("name");
    if (name == nullptr) return std::nullopt;
    const std::string& n = *name;
    if (n == "IsAnticipating") return Operation(IsAnticipating{});
    if (n == "IsLanding") return Operation(IsLanding{});
    if (n == "IsManualing") return Operation(IsManualing{});
    if (n == "IsDoingTrick") return Operation(IsDoingTrick{});
    //82BC7618 fixes this operation's attribute to disttocog.
    if (n == "SetLandingData") return Operation(SetLandingData{});
    if (n == "DisableTricks") {
        const auto* v = a.get("length");
        return Operation(DisableTricks{v ? rs::from_bits(v->float_bits) : 0.5f});
    }
    if (n == "ChooseRandomLanding") {
        //82BC7788 truncates the configured float toward zero.
        const auto* v = a.get("numlandings");
        return Operation(ChooseRandomLanding{rs::as_i32(v ? rs::from_bits(v->float_bits) : 1.0f)});
    }
    return std::nullopt;
}

std::optional<Condition> Condition::parse(const Attributes& a) {
    const std::string* name = a.text("name");
    if (name == nullptr) return std::nullopt;
    const std::string& n = *name;
    if (n == "HasLandingType") {
        const std::string* t = a.text("landingType");
        const std::string kind = t ? *t : std::string("straight");
        return Condition(HasLandingType{kind == "spin" ? 2u : kind == "sketchy" ? 1u : 0u});
    }
    if (n == "HasTiltToLargeForPreland") return Condition(HasTiltToLargeForPreland{});
    if (n == "AllowedToTrick") return Condition(AllowedToTrick{});
    return std::nullopt;
}

rs::Result<bool, std::string> Condition::evaluate(std::optional<Physical> physical, const Flags& flags,
                                                  std::optional<bool> override_preland) const {
    if (const auto* c = std::get_if<HasLandingType>(&value)) {
        if (!physical) return rs::make_err(std::string("HasLandingType requires Animation96"));
        return physical->kind == c->_0;
    }
    if (std::holds_alternative<HasTiltToLargeForPreland>(value)) {
        if (!override_preland) {
            return rs::make_err(std::string("HasTiltToLargeForPreland requires original prelanding output"));
        }
        return *override_preland;
    }
    //82BC5860 ->8231F460+48 ->82BA74D8 ->specific v116.
    return flags.tricks_allowed;
}

}  // namespace s3::game::graph_host::motion_landing
