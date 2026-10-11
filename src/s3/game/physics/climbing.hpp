// Port of skate-game/src/physics/climbing/mod.rs
//! Hybrid ledge climbing. This extension owns movement while attached; the
//! stock action/motion graphs resume on release. It does not forge stock states
//! or animation-bank records, and uses the same skeleton and render snapshots.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/animation/output/native.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/animation_pose/glam.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/climbing/approach.hpp"
#include "s3/game/physics/climbing/clip.hpp"
#include "s3/game/physics/climbing/contacts.hpp"
#include "s3/game/physics/climbing/ledge.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/transform_shim.hpp"

namespace s3::game::physics::climbing {

using ::s3::animation::output::NativeMatrix;
using clip::blend;
using clip::Clips;
using glam::Mat4;
using glam::Vec3;
using transform_shim::Transform;

enum class Phase {
    Catch,
    Hang,
    Mantle,
    Settle,
};

struct Attached {
    Phase phase = Phase::Catch;
    float time = 0.0f;
    ledge::Ledge ledge{};
    Transform start_root{};
    std::vector<Transform> entry;
    std::vector<NativeMatrix> fallback;
    Mat4 board_world{};
    Mat4 physical_board_world{};
    bool carry_board = false;
};

/// `#[derive(Default)]`.
struct Runtime {
    std::optional<Clips> clips{};
    std::vector<std::size_t> indices{};
    std::optional<Attached> active{};
    float cooldown = 0.0f;
    std::optional<approach::Approach> approach{};
    std::vector<Transform> ground_entry{};

    /// `root: &Path` -> (vfs, asset root).
    static rs::Result<Runtime, std::string> load(const sd::Vfs& vfs, const std::string& root,
                                                 const std::vector<std::string>& names);
    rs::Result<bool, std::string> advance(GamePhysics& physics, SkaterRuntime& skater, const PlayerControls& controls,
                                          camera::CameraRuntime& camera);
};

Mat4 matrix(const NativeMatrix& m);
NativeMatrix native(const Mat4& m);
std::array<float, 4> point(Vec3 p);
float smooth(float t);
bool pressed(const PlayerControls& controls, std::uint32_t bit);

/// Called before native animation/input/solve. Attached motion has a single
/// owner; no gravity, foot IK, or stock root motion competes with hand contact.
rs::Result<bool, std::string> advance(GamePhysics& physics, SkaterRuntime& skater, const PlayerControls& controls,
                                      camera::CameraRuntime& camera);

void zero_rates(::s3::physics::rigid_body::RetailBodyRates& r);
Status publish_pose(const GamePhysics& physics, SkaterRuntime& skater, Mat4 root, std::vector<NativeMatrix> pose);

}  // namespace s3::game::physics::climbing
