// Port of skate-game/src/physics/wipeout_states/ragdoll/settings.rs
//! Authored inputs to normal82BE58A8 and ragdoll82BE59A8/6D60.
#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "s3/core/physics/contact.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::wipeout_states::ragdoll::settings {

struct Settings {
    std::array<std::array<std::uint32_t, 4>, 22> normal_limits{};
    std::array<std::array<std::uint32_t, 4>, 22> ragdoll_limits{};
    bool inverse_mass = false;
    bool inverse_inertia = false;
    std::array<float, 2> drag{};
    std::array<::s3::physics::contact::RetailContactMaterial, 2> materials{};

    static rs::Result<Settings, std::string> load(const sd::Collections& data, const sd::Vfs& vfs,
                                                  const std::string& assets, const std::string& bank_sha);
};

std::array<std::uint32_t, 4> limits(float swing, float twist);

}  // namespace s3::game::physics::wipeout_states::ragdoll::settings
