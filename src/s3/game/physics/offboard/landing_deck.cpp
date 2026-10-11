// Port of skate-game/src/physics/offboard/landing_deck.rs (+ input.rs, output.rs, query.rs)
#include "s3/game/physics/offboard/landing_deck.hpp"

#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/player_input.hpp"

namespace s3::game::physics::offboard::landing_deck {

rs::Result<Owner, std::string> Owner::load(const sd::Collections& data) {
    Owner out{};
    out.manager = Manager{};
    S3G_TRY(out.settings.deck_min_uprightness, data.float_("physics_landingondeck", "default", "DeckMinUprightness"));
    S3G_TRY(out.settings.approximate_com_height,
            data.float_("physics_landingondeck", "default", "ApproxCOMHeightOnLanding"));
    out.completion = std::nullopt;
    return out;
}

void Owner::reset() { manager.reset(); }

Status Owner::assist(const StaticScene& scene, const PlayerInputRuntime& input, float maximum_velocity_change) {
    Manager next = manager;
    S3G_TRY(native::Input processed, input::processed(input));
    const AssistInput assist_input{processed, input::position(input), input::velocity(input), maximum_velocity_change};
    const auto request = next.assist(assist_input, settings);
    std::optional<QueryResult> completion;
    if (request.has_value()) {
        S3G_TRY(completion, query::execute(scene, *request, input.processed));
    }
    if (completion.has_value()) {
        //Native Assist starts a new batch even if an older one was pending.
        this->completion = completion;
        next.query_submitted();
    }
    manager = next;
    return ok();
}

rs::Result<UpdateOutput, std::string> Owner::update(const StaticScene& scene, const PlayerInputRuntime& input) {
    Manager next = manager;
    S3G_TRY(native::Input processed, input::processed(input));
    UpdateOutput output = next.update(processed, settings);
    std::optional<QueryResult> completion;
    if (output.query.has_value()) {
        S3G_TRY(completion, query::execute(scene, *output.query, input.processed));
    }
    if (completion.has_value()) {
        this->completion = completion;
        next.query_submitted();
    }
    manager = next;
    return output;
}

rs::Result<Vector, std::string> Owner::calculate_accurate_ik_offset(const PlayerInputRuntime& input,
                                                                    const AnimatedSkeleton& animation,
                                                                    Vector mapped_position_12608, float time_to_land) {
    S3G_TRY(native::Input processed, input::processed(input));
    return manager.calculate_accurate_ik_offset(IkInput{processed, time_to_land, animation.roots.animation_to_world,
                                                        animation.record.centre_of_mass, mapped_position_12608});
}

native::FillOutput Owner::fill() const { return manager.fill(); }

// ---- input.rs ----
namespace input {
rs::Result<native::Input, std::string> processed(const PlayerInputRuntime& input) {
    const auto& p = input.processed;
    if (!input.toolkit.has_value()) {
        return rs::make_err(std::string("Landing manager requires the current processed BoardToolkit"));
    }
    const auto& board = *input.toolkit;
    return native::Input{
        board.deck[1],
        board.deck[3],
        rs::from_bits(p.vectors_400_416[0]),
        rs::from_bits(p.vectors_544_560_592_608[0]),
        static_cast<std::int32_t>(p.external_physics_1616.flags),
        p.flags_2480,
        p.surface_mode_2540,
        p.wheel_count_2556,
    };
}
Vector position(const PlayerInputRuntime& input) { return rs::from_bits(input.processed.vectors_544_560_592_608[2]); }
Vector velocity(const PlayerInputRuntime& input) { return rs::from_bits(input.processed.vectors_544_560_592_608[3]); }
}  // namespace input

// ---- output.rs ----
void Owner::publish(PublishTargets output) const {
    const auto values = manager.fill();
    output.off_board->flag_316 = static_cast<std::uint8_t>(values.can_land_316);
    output.off_board->hippy_hurdling_317 = static_cast<std::uint8_t>(values.hippy_hurdling_317);
    if (values.moving_contact.has_value()) {
        *output.skeleton_flag_3482 = 1;
        *output.skeleton_position_48 = rs::to_bits(*values.moving_contact);
    }
    //Native261 false leaves48 AND3482 untouched.
}

// ---- query.rs ----
namespace query {
rs::Result<QueryResult, std::string> execute(const StaticScene& scene, native::QueryRequest request,
                                             const sc::player::input_phase::ProcessedPhysicsInput& processed) {
    //79948 stack54=processed2952; stack5C=0; stack64=null.
    //8276E280 writes request80/84 from stack54/5C.8276DBE0 forwards
    //these to82770B40's matching-group and mesh-rejection checks.
    //Use the accelerated arc provider, NOT Ground's sweep-oriented Scene batch.
    auto r = scene.trajectory(request, static_cast<std::int32_t>(processed.actor_query_2952), 0);
    if (r.is_err()) return rs::make_err(std::string(r.unwrap_err()));
    return r.unwrap();
}
}  // namespace query

Status Owner::submit(const StaticScene& scene, native::QueryRequest request,
                     const sc::player::input_phase::ProcessedPhysicsInput& processed) {
    S3G_TRY(QueryResult result, query::execute(scene, request, processed));
    completion = result;
    manager.query_submitted();
    return ok();
}

Status Owner::post_physics(const PlayerInputRuntime& input) {
    if (!manager.pending_262) {
        if (completion.has_value()) {
            return rs::make_err(std::string("Landing query completion exists without native pending262"));
        }
        return ok();
    }
    if (!completion.has_value()) {
        return rs::make_err(std::string("Landing pending262 has no submitted world-query completion"));
    }
    const QueryResult done = *completion;
    const auto& p = input.processed;
    auto r = manager.sync<std::string>(
        done.valid() ? std::optional<QueryResult>(done) : std::nullopt,
        native::SyncInput{input::position(input), p.flags_2488},
        [&](const Manager&) -> rs::Result<Vector, std::string> {
            //Only the original79E30 gate inside Sync invokes this existing
            //numeric provider. No state503, custom branch, or synthetic result.
            S3G_TRY(native::Input processed, input::processed(input));
            namespace hj = ::s3::player::offboard::hippy_jump;
            return hj::calculate(hj::Input{
                rs::from_bits(p.vectors_880_896_912_928_944[3][1]) + rs::from_bits(0x3e4ccccdu),
                processed.board_position_112,
                input::position(input),
                processed.up_544,
                processed.board_velocity_400,
            });
        });
    if (r.is_err()) return rs::make_err(r.unwrap_err());
    completion = std::nullopt;
    return ok();
}

}  // namespace s3::game::physics::offboard::landing_deck
