// Port of skate-game/src/physics/grind.rs.
//! ZIP grind runtime split into source-closed components against CURRENT APIs.
//! Physical acquisition remains with the input manager. The retained runtime
//! owns family state, body operations and publication, never candidate selection.
//!
//! `enum Family` lives in grind/family.hpp (included here) so the submodule headers
//! can use it without including this umbrella header.
#pragma once
#include "s3/game/physics/grind/family.hpp"

#include "s3/game/physics/grind/acquisition.hpp"
#include "s3/game/physics/grind/advance.hpp"
#include "s3/game/physics/grind/arithmetic.hpp"
#include "s3/game/physics/grind/board.hpp"
#include "s3/game/physics/grind/condition.hpp"
#include "s3/game/physics/grind/contact.hpp"
#include "s3/game/physics/grind/forces.hpp"
#include "s3/game/physics/grind/involuntary.hpp"
#include "s3/game/physics/grind/launch.hpp"
#include "s3/game/physics/grind/lifecycle.hpp"
#include "s3/game/physics/grind/observation.hpp"
#include "s3/game/physics/grind/output.hpp"
#include "s3/game/physics/grind/phase.hpp"
#include "s3/game/physics/grind/post.hpp"
#include "s3/game/physics/grind/rng.hpp"
#include "s3/game/physics/grind/runtime.hpp"
#include "s3/game/physics/grind/settings.hpp"
#include "s3/game/physics/grind/state.hpp"
#include "s3/game/physics/grind/substate.hpp"
#include "s3/game/physics/grind/tip_nudge.hpp"
#include "s3/game/physics/grind/wipeout_output.hpp"

namespace s3::game::physics::grind {

// C++: the private modules `advance`, `condition`, `post` are namespaces `advance_`, `condition_`, `post_`
// because a namespace and a function cannot share a name in one scope.
using advance_::advance;
using condition_::condition;
using phase::enter;
using phase::exit;
using phase::fill;
using phase::post;
using runtime::Runtime;
using observation::ManagerObservation;

}  // namespace s3::game::physics::grind
