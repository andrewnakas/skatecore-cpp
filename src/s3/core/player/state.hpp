// Port of skate-core/src/player/state.rs (complete file).
// Physical-player state identities and native object ownership from
// `PhysicalPlayer` construction and `SetPhysicsState` (`0x82DB8540`).
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::state {

enum class PhysicalStateId : std::uint32_t {
    PhysicsGround = 100,
    SlideGround = 101,
    RevertGround = 102,
    GroundAnimation = 103,
    Skitching = 104,
    FollowPath = 105,
    PhysicsAir = 200,
    KnownAir = 201,
    PhysicsAirSecondary = 202,
    WipeoutGround = 300,
    GrindBoardslide = 400,
    GrindFiftyFifty = 401,
    GrindTipslide = 402,
    GrindFiveO = 403,
    GrindBackslash = 404,
    GrindDarkslide = 405,
    BipedGround = 500,
    BipedAir = 501,
    OffBoardPushing = 502,
    LandingOnDeck = 503,
    HandPlant = 600,
    FootPlant = 601,
    Boneless = 602,
    Sleeping = 700,
    Nonspecific = 701,
    Teleporting = 702,
};

struct UnknownPhysicalState {
    std::uint32_t value = 0;
};
S3_DERIVE_EQ(UnknownPhysicalState, value)

/// `impl TryFrom<u32> for PhysicalStateId`.
inline rs::Result<PhysicalStateId, UnknownPhysicalState> try_from(std::uint32_t value) {
    switch (value) {
        case 100: case 101: case 102: case 103: case 104: case 105:
        case 200: case 201: case 202:
        case 300:
        case 400: case 401: case 402: case 403: case 404: case 405:
        case 500: case 501: case 502: case 503:
        case 600: case 601: case 602:
        case 700: case 701: case 702:
            return static_cast<PhysicalStateId>(value);
        default:
            return rs::make_err(UnknownPhysicalState{value});
    }
}

inline constexpr std::array<PhysicalStateId, 26> ALL = {
    PhysicalStateId::PhysicsGround,   PhysicalStateId::SlideGround,
    PhysicalStateId::RevertGround,    PhysicalStateId::GroundAnimation,
    PhysicalStateId::Skitching,       PhysicalStateId::FollowPath,
    PhysicalStateId::PhysicsAir,      PhysicalStateId::KnownAir,
    PhysicalStateId::PhysicsAirSecondary, PhysicalStateId::WipeoutGround,
    PhysicalStateId::GrindBoardslide, PhysicalStateId::GrindFiftyFifty,
    PhysicalStateId::GrindTipslide,   PhysicalStateId::GrindFiveO,
    PhysicalStateId::GrindBackslash,  PhysicalStateId::GrindDarkslide,
    PhysicalStateId::BipedGround,     PhysicalStateId::BipedAir,
    PhysicalStateId::OffBoardPushing, PhysicalStateId::LandingOnDeck,
    PhysicalStateId::HandPlant,       PhysicalStateId::FootPlant,
    PhysicalStateId::Boneless,        PhysicalStateId::Sleeping,
    PhysicalStateId::Nonspecific,     PhysicalStateId::Teleporting,
};

/// Offset of the owning state pointer inside `PhysicalPlayer`.
constexpr std::uint32_t native_owner_offset(PhysicalStateId self) {
    switch (self) {
        case PhysicalStateId::Sleeping: return 1692;
        case PhysicalStateId::PhysicsGround: return 1696;
        case PhysicalStateId::SlideGround: return 1700;
        case PhysicalStateId::RevertGround: return 1704;
        case PhysicalStateId::GroundAnimation: return 1708;
        case PhysicalStateId::PhysicsAir: return 1712;
        case PhysicalStateId::PhysicsAirSecondary: return 1716;
        case PhysicalStateId::KnownAir: return 1720;
        case PhysicalStateId::GrindDarkslide: return 1724;
        case PhysicalStateId::GrindBoardslide: return 1728;
        case PhysicalStateId::GrindFiftyFifty: return 1732;
        case PhysicalStateId::GrindTipslide: return 1736;
        case PhysicalStateId::GrindFiveO: return 1740;
        case PhysicalStateId::GrindBackslash: return 1744;
        case PhysicalStateId::WipeoutGround: return 1748;
        case PhysicalStateId::Nonspecific: return 1752;
        case PhysicalStateId::Teleporting: return 1756;
        case PhysicalStateId::FollowPath: return 1760;
        case PhysicalStateId::BipedGround: return 1764;
        case PhysicalStateId::BipedAir: return 1768;
        case PhysicalStateId::Skitching: return 1772;
        case PhysicalStateId::OffBoardPushing: return 1776;
        case PhysicalStateId::HandPlant: return 1780;
        case PhysicalStateId::FootPlant: return 1784;
        case PhysicalStateId::Boneless: return 1788;
        case PhysicalStateId::LandingOnDeck: return 1792;
    }
    return 0;
}

/// Category written by native `GetStateCategory` into ProcessedPhysIn.
constexpr std::uint32_t category(PhysicalStateId self) {
    return (static_cast<std::uint32_t>(self) / 100) * 100;
}

constexpr bool is_grind(PhysicalStateId self) {
    switch (self) {
        case PhysicalStateId::GrindBoardslide:
        case PhysicalStateId::GrindFiftyFifty:
        case PhysicalStateId::GrindTipslide:
        case PhysicalStateId::GrindFiveO:
        case PhysicalStateId::GrindBackslash:
        case PhysicalStateId::GrindDarkslide:
            return true;
        default:
            return false;
    }
}

}  // namespace s3::player::state
