// Port of skate-game/src/graph_host/motion_grind/animation_owner.rs
// (`#[path = "motion_grind/animation_owner.rs"] mod grind_owner;` of motion_animation).
//! Compile as a child of motion_animation, NOT motion_grind, so the real
//! owner's private current tree stays private. See the module line in handoff.
//
// The Rust module is `impl motion_grind::animation::Animation for MotionAnimation`. That trait is
// ported as a compile-time concept (motion_grind/animation.hpp); MotionAnimation satisfies it with
// its member functions set_attribute, apply_parameters, query_current_attribute (defined in
// grind_owner.cpp), emit_packet and intent.
#pragma once
#include "s3/game/graph_host/motion_animation.hpp"
