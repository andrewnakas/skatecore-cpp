// Rust `{:?}` of skate_core::player::state::PhysicalStateId (derived Debug: the variant name).
#pragma once
#include "s3/core/player/state.hpp"

namespace s3::game {

inline const char* debug_name(::s3::player::state::PhysicalStateId id) {
    using P = ::s3::player::state::PhysicalStateId;
    switch (id) {
        case P::PhysicsGround: return "PhysicsGround";
        case P::SlideGround: return "SlideGround";
        case P::RevertGround: return "RevertGround";
        case P::GroundAnimation: return "GroundAnimation";
        case P::Skitching: return "Skitching";
        case P::FollowPath: return "FollowPath";
        case P::PhysicsAir: return "PhysicsAir";
        case P::KnownAir: return "KnownAir";
        case P::PhysicsAirSecondary: return "PhysicsAirSecondary";
        case P::WipeoutGround: return "WipeoutGround";
        case P::GrindBoardslide: return "GrindBoardslide";
        case P::GrindFiftyFifty: return "GrindFiftyFifty";
        case P::GrindTipslide: return "GrindTipslide";
        case P::GrindFiveO: return "GrindFiveO";
        case P::GrindBackslash: return "GrindBackslash";
        case P::GrindDarkslide: return "GrindDarkslide";
        case P::BipedGround: return "BipedGround";
        case P::BipedAir: return "BipedAir";
        case P::OffBoardPushing: return "OffBoardPushing";
        case P::LandingOnDeck: return "LandingOnDeck";
        case P::HandPlant: return "HandPlant";
        case P::FootPlant: return "FootPlant";
        case P::Boneless: return "Boneless";
        case P::Sleeping: return "Sleeping";
        case P::Nonspecific: return "Nonspecific";
        case P::Teleporting: return "Teleporting";
    }
    return "?";
}

}  // namespace s3::game
