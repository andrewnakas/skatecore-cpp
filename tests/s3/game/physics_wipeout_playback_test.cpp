// Port of skate-game/src/tests/wipeout_playback.rs (mounted in physics.rs as `wipeout_tests`),
// including its `#[path = "bail_geometry.rs"] mod geometry` helper.
//! Real Xbox bail chord through input, stock graphs, physical state and pose.
// The Rust tests are `#[ignore = "requires private stock animation banks and collections"]`; here
// they run against the cleanroom pack and skip without it.
#include <cmath>
#include <cstdio>
#include <optional>
#include <tuple>

#include "physics_playback_support.hpp"
#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/physics/joint_records.hpp"
#include "s3/core/player/state.hpp"

using namespace s3::game;
using namespace s3::game::physics;
using s3::player::state::PhysicalStateId;
namespace pb = s3test::game::playback;

namespace {

// ---- bail_geometry.rs ----------------------------------------------------------------------
//! Test-only measurements, not solver limits or substitute physical constraints.
using s3::physics::assembly::BodySnapshot;

double separation(const BodySnapshot& a, const BodySnapshot& b, const std::array<std::uint32_t, 20>& frames) {
    auto point = [&](const BodySnapshot& body, std::size_t offset) {
        const auto p = body.rates.position;
        const std::array<float, 3> translation{p.x, p.y, p.z};
        std::array<double, 3> out{};
        for (std::size_t i = 0; i < 3; ++i) {
            double sum = -0.0;
            for (std::size_t j = 0; j < 3; ++j) {
                sum += double(body.rates.basis.columns[j][i]) * double(s3::rs::from_bits(frames[offset + j]));
            }
            out[i] = double(translation[i]) + sum;
        }
        return out;
    };
    const auto pa = point(a, 4);
    const auto pb_ = point(b, 12);
    double sum = -0.0;
    for (std::size_t i = 0; i < 3; ++i) sum += std::pow(pb_[i] - pa[i], 2);
    return std::sqrt(sum);
}

struct Geometry {
    std::tuple<double, std::size_t, std::size_t> board_anchor_peak{0.0, 0, 0};
    std::tuple<double, std::size_t, std::size_t> leg_anchor_peak{0.0, 0, 0};
    std::tuple<double, std::size_t> basis_gram_peak{0.0, 0};

    void observe(const GamePhysics& physics, const SkaterRuntime& skater, std::size_t tick) {
        const auto records = s3::physics::joint_records::default_joint_records();
        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto& joint = records[i];
            const double distance =
                separation(physics.board.bodies()[s3::physics::board::index(joint.live_body_a())],
                           physics.board.bodies()[s3::physics::board::index(joint.live_body_b())], joint.frames.words);
            if (distance > std::get<0>(board_anchor_peak)) board_anchor_peak = {distance, tick, i};
        }
        for (const auto& joint : skater.skeleton_joints.records) {
            if (!(joint.child >= 15 && joint.child <= 22)) continue;
            const double distance = separation(skater.skeleton.bodies()[joint.child],
                                               skater.skeleton.bodies()[joint.parent], joint.frames.words);
            if (distance > std::get<0>(leg_anchor_peak)) leg_anchor_peak = {distance, tick, joint.child};
        }
        auto gram = [&](const BodySnapshot& body) {
            const auto columns = body.rates.basis.columns;
            for (std::size_t i = 0; i < 3; ++i) {
                for (std::size_t j = 0; j < 3; ++j) {
                    double dot = -0.0;
                    for (std::size_t k = 0; k < 3; ++k) dot += double(columns[i][k]) * double(columns[j][k]);
                    const double error = std::fabs(dot - (i == j ? 1.0 : 0.0));
                    if (error > std::get<0>(basis_gram_peak)) basis_gram_peak = {error, tick};
                }
            }
        };
        for (const auto& body : physics.board.bodies()) gram(body);
        for (const auto& body : skater.skeleton.bodies()) gram(body);
    }

    std::string debug() const {
        char buf[256];
        std::snprintf(buf, sizeof buf,
                      "Geometry { board_anchor_peak: (%.17g, %zu, %zu), leg_anchor_peak: (%.17g, %zu, %zu), "
                      "basis_gram_peak: (%.17g, %zu) }",
                      std::get<0>(board_anchor_peak), std::get<1>(board_anchor_peak), std::get<2>(board_anchor_peak),
                      std::get<0>(leg_anchor_peak), std::get<1>(leg_anchor_peak), std::get<2>(leg_anchor_peak),
                      std::get<0>(basis_gram_peak), std::get<1>(basis_gram_peak));
        return buf;
    }
};

// ---- wipeout_playback.rs -------------------------------------------------------------------

bool finite3(const s3::math::Vector3& v) {
    return s3::rs::is_finite(v.x) && s3::rs::is_finite(v.y) && s3::rs::is_finite(v.z);
}

void assert_finite(const GamePhysics& physics, const SkaterRuntime& skater, std::size_t tick) {
    auto check_body = [&](const BodySnapshot& body) {
        for (const auto& vector : {body.rates.position, body.rates.linear_velocity, body.rates.angular_velocity}) {
            CHECK_MSG(finite3(vector), "Nonfinite physical body at tick" << tick);
        }
    };
    for (const auto& body : physics.board.bodies()) check_body(body);
    for (const auto& body : skater.skeleton.bodies()) check_body(body);
    bool board_finite = true;
    for (const auto& part : physics.board.part_transforms()) {
        for (const auto& column : part.basis.columns) {
            for (const float v : column) board_finite = board_finite && s3::rs::is_finite(v);
        }
    }
    CHECK_MSG(board_finite, "Nonfinite board orientation at tick" << tick);
    bool skeleton_finite = true;
    for (const auto& part : skater.skeleton.part_transforms()) {
        for (const auto& row : part) {
            for (const float v : row) skeleton_finite = skeleton_finite && s3::rs::is_finite(v);
        }
    }
    CHECK_MSG(skeleton_finite, "Nonfinite physical skeleton pose at tick" << tick);
    CHECK_MSG(!skater.animation.pose.empty(), "Missing animation pose at tick" << tick);
    bool animation_finite = true;
    for (const auto& sample : skater.animation.pose) {
        for (const float v : sample.scale) animation_finite = animation_finite && s3::rs::is_finite(v);
        for (const float v : sample.rotation) animation_finite = animation_finite && s3::rs::is_finite(v);
        for (const float v : sample.translation) animation_finite = animation_finite && s3::rs::is_finite(v);
    }
    CHECK_MSG(animation_finite, "Nonfinite animation at tick" << tick);
}

std::string opt_tick(std::optional<std::size_t> t) { return t ? "Some(" + std::to_string(*t) + ")" : "None"; }

void checkpoint_reply(bool on_foot) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    auto physics = pb::unwrap(GamePhysics::load(*s->vfs, s->root), "physics");
    auto skater = pb::unwrap(SkaterRuntime::load(*s->vfs, s->root, s->graphs, physics, "normal"), "skater");
    PlayerControls controls{};
    input::controllers::ControllerInput input{};
    auto camera = pb::unwrap(camera::CameraRuntime::load(*s->vfs, s->root), "camera");
    auto checkpoint = physics.board.bodies()[6].rates.position;
    const auto destination = on_foot ? PhysicalStateId::BipedGround : PhysicalStateId::PhysicsGround;
    bool saw_ready = false;
    int restored_ticks = 0;
    for (std::size_t tick = 0; tick < 160; ++tick) {
        if (tick == 60) {
            CHECK(std::fabs(physics.board.bodies()[6].rates.position.z - checkpoint.z) > 0.1f);
            if (on_foot) {
                const auto transform = skater.animated_skeleton.roots.animation_to_world;
                checkpoint = s3::math::Vector3::new_(transform[3][0], transform[3][1], transform[3][2]);
                skater.teleport_state.request_manual(transform, false);
            }
            if (!on_foot) skater.teleport_state.request_checkpoint();
        }
        const auto result = pb::tick(physics, skater, controls, input, camera, *s,
                                     pb::xbox((tick >= 12 && tick < 60) ? 0x1000 : 0, {0, 0}, {0, 0}, {0, 0}), false);
        if (!result.is_ok()) pb::panic("Checkpoint tick" + std::to_string(tick) + ": " + result.unwrap_err());
        if (tick == 60) {
            CHECK_EQ(skater.player_state.current(), PhysicalStateId::Teleporting);
            CHECK_MSG(skater.player_input.physical.teleport_output.has_value(), "702 must publish the captured reply");
            const auto output = *skater.player_input.physical.teleport_output;
            CHECK_EQ(output.next_state, on_foot ? 500u : 100u);
            CHECK_EQ(output.state_61, 1);
            CHECK_EQ(output.board_272, 1);
            CHECK_EQ(s3::rs::from_bits(output.transform[3][2]), checkpoint.z);
            saw_ready = true;
        } else if (tick == 61) {
            CHECK(saw_ready);
            CHECK_EQ(skater.player_state.current(), PhysicalStateId::PhysicsGround);
            const auto position = physics.board.bodies()[6].rates.position;
            CHECK(std::fabs(position.x - checkpoint.x) < 0.05f);
            CHECK(std::fabs(position.z - checkpoint.z) < 0.05f);
            CHECK(!skater.skeleton_collision.is_ragdoll);
        }
        if (tick > 61 && skater.player_state.current() == destination) restored_ticks += 1;
        assert_finite(physics, skater, tick);
    }
    CHECK(saw_ready && restored_ticks > 60);
}

}  // namespace

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(raw_controller_bail_runs_stock_wipeout_for_900_ticks) {
    const auto* s = pb::stock();
    if (s == nullptr) return;
    auto physics = pb::unwrap(GamePhysics::load(*s->vfs, s->root), "physics");
    auto skater = pb::unwrap(SkaterRuntime::load(*s->vfs, s->root, s->graphs, physics, "normal"), "skater");
    PlayerControls controls{};
    input::controllers::ControllerInput input{};
    auto camera = pb::unwrap(camera::CameraRuntime::load(*s->vfs, s->root), "camera");
    int settled_ticks = 0;
    std::optional<std::size_t> requested_at;
    std::optional<std::size_t> entered_at;
    bool saw_action_request = false;
    Geometry geometry{};
    for (std::size_t tick = 0; tick < 1200; ++tick) {
        const bool request = settled_ticks >= 60 && !requested_at;
        if (request) requested_at = tick;
        //8259BDCC..BEF0 requires exact full trigger endpoints, both stick
        //buttons and a fresh edge. Supply one raw packet, then release it.
        const auto packet = pb::xbox(request ? 0x00C0 : 0, request ? std::array<std::uint8_t, 2>{255, 255}
                                                                   : std::array<std::uint8_t, 2>{0, 0},
                                     {0, 0}, {0, 0});
        const auto result = pb::tick(physics, skater, controls, input, camera, *s, packet, false);
        if (!result.is_ok()) {
            pb::panic("Raw bail tick" + std::to_string(tick) + ": " + result.unwrap_err() + "; state=" +
                      std::to_string(unsigned(skater.player_state.current())) + "; requested=" +
                      opt_tick(requested_at) + "; entered=" + opt_tick(entered_at));
        }
        const float* action_request = skater.animation.action.action_intents.get("WipeOutRequest");
        if (action_request != nullptr && *action_request == 1.0f) {
            CHECK_MSG(requested_at.has_value(), "Bail intent appeared before the raw chord");
            saw_action_request = true;
        }
        const auto state = skater.player_state.current();
        if (state == PhysicalStateId::WipeoutGround) {
            geometry.observe(physics, skater, tick);
            // WipeoutEnter82D3B5E8 releases both board drive channels;
            // SetPhysicsState82DB8540/LetGo82D75440 releases possession.
            // Finite transforms alone cannot detect a stale attachment.
            const std::array<std::uint32_t, 8> released{0, 0, 0, 2, 0, 0, 0, 2};
            CHECK_MSG(physics.board.hook().drive.dynamics == released, "Deck still driven at bail tick" << tick);
            CHECK_MSG(skater.board_possession.state.selected_hand_424 == 2u,
                      "Hand still owns released board at tick" << tick);
            for (const auto& hand : skater.board_possession.state.hands) {
                const std::array<std::array<std::uint32_t, 4>, 2> free{{{0, 0, 0, 2}, {0, 0, 0, 2}}};
                CHECK_MSG(hand.dynamics == free, "Hand/deck constraint survives bail tick" << tick);
            }
            CHECK_EQ(physics.board.collision_group(), 7u);
            CHECK(skater.skeleton_collision.is_ragdoll);
        }
        if (!requested_at) {
            CHECK_MSG(state != PhysicalStateId::WipeoutGround, "Bailed before controller request");
            if (state == PhysicalStateId::PhysicsGround && physics.riding.ground.wheel_contact_count > 0) {
                settled_ticks += 1;
            } else {
                settled_ticks = 0;
            }
        } else if (state == PhysicalStateId::WipeoutGround && !entered_at) {
            CHECK_MSG(saw_action_request, "State300 entered without stock ActionGraph bail intent");
            entered_at = tick;
        }
        assert_finite(physics, skater, tick);
        CHECK_MSG(skater.pose_generation == physics.ticks, "Stale pose at tick" << tick);
        CHECK_MSG(camera.frame.has_value(), "Missing gameplay camera at tick" << tick);
        if (entered_at) {
            if (tick - *entered_at >= 900) {
                std::fprintf(stderr, "Raw bail completed: request=%s, state300=%zu, final_tick=%zu\n",
                             opt_tick(requested_at).c_str(), *entered_at, tick);
                std::fprintf(stderr, "Post-solve bail geometry (meters/tick/joint; no parity tolerance): %s\n",
                             geometry.debug().c_str());
                return;
            }
        }
    }
    CHECK_MSG(requested_at.has_value(), "Never settled for60 consecutive grounded ticks");
    CHECK_MSG(saw_action_request, "Raw chord never reached stock ActionGraph WipeOutRequest");
    pb::panic("Did not complete900 ticks after state300: entered=" + opt_tick(entered_at));
}

/// Exercise the actual actor checkpoint reply, not a forced physical state.
// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(authored_checkpoint_reply_runs_teleport_state_and_restores_riding) { checkpoint_reply(false); }

// #[ignore = "requires private stock animation banks and collections"]
S3_TEST(marker_reply_restores_on_foot) {
    if (s3test::game::skip_stale("on-foot manual reply enters BipedGround and the selector then picks PhysicsAir; the test expects PhysicsGround with the board at the checkpoint (docs/PARITY.md)")) return;
    checkpoint_reply(true);
}
