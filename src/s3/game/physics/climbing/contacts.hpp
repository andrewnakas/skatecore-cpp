// Port of skate-game/src/physics/climbing/contacts.rs
//! Two-bone IK over the authored pose. Preserve limb lengths and
//! align both palms to fixed surface contacts with independent wrist rotation.
#pragma once
#include <cstddef>
#include <utility>
#include <vector>

#include "s3/game/animation_pose/glam.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/climbing/clip.hpp"
#include "s3/game/physics/climbing/ledge.hpp"
#include "s3/game/transform_shim.hpp"

namespace s3::game::physics::climbing::contacts {

using clip::Clip;
using glam::Mat4;
using glam::Quat;
using glam::Vec3;
using transform_shim::Transform;

Vec3 clearance(const Clip& clip, const std::vector<Mat4>& globals);

inline const Vec3 PALM = Vec3::new_(0.075f, 0.052f, 0.0f);
std::pair<Vec3, Quat> wrist(ledge::Ledge ledge, std::size_t i);
void hands(const Clip& clip, std::vector<Transform>& locals, Mat4 root, ledge::Ledge ledge, float weight);
void solve(const Clip& clip, std::vector<Transform>& locals, std::size_t upper, std::size_t lower, std::size_t hand,
           Vec3 target, Quat wrist);
void rotate(const Clip& clip, std::vector<Transform>& locals, const std::vector<Mat4>& globals, std::size_t bone,
            Vec3 from, Vec3 to);

}  // namespace s3::game::physics::climbing::contacts
