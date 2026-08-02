// B5 acceptance: scripted approach + turn while carrying → HashState pin.
#include <doctest/doctest.h>

#include "core/hash.hpp"
#include "core/match_engine.hpp"
#include "core/movement.hpp"
#include "core/possession.hpp"
#include "core/match_input.hpp"

using namespace at;

namespace {

uint64_t RunDribbleTurn() {
    MatchEngine eng;
    eng.Reset(0xB5000001u);
    eng.Step(MatchInput{});

    MatchState s = eng.State();
    s.sides[0].control.player_number = 1;
    s.sides[0].control.ball_can_be_controlled = 1;
    s.sides[0].control.controlled_slot = 9;
    s.sides[0].squad[9].attrs.speed = 5;
    s.sides[0].squad[9].attrs.ball_control = 4;
    for (int r = 0; r < kMatchTacticRoles; ++r)
        for (int q = 0; q < kMatchBallQuadrants; ++q)
            s.sides[0].tactics.cells[static_cast<size_t>(r)][static_cast<size_t>(q)] =
                static_cast<uint8_t>(((r % 15) << 4) | (q % 16));

    PlacePlayersAtKickoff(s);
    // Ball next to striker.
    Entity& striker = s.players[9];
    s.ball.pos.x = striker.pos.x;
    s.ball.pos.y = striker.pos.y;
    s.ball.pos.z = Fix{};
    s.ball.speed = 0;
    s.sides[0].control.ball_can_be_controlled = 1;

    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.clock.stoppage_event_timer = 0;
    s.phase = MatchPhase::InPlay;
    eng.LoadState(s);

    MatchInput in{};
    // Approach / settle possession while moving east, then turn south.
    for (int i = 0; i < 40; ++i) {
        in.p1.dir = Dir::E;
        eng.Step(in);
    }
    for (int i = 0; i < 40; ++i) {
        in.p1.dir = Dir::S;
        eng.Step(in);
    }
    return HashState(eng.State());
}

} // namespace

TEST_CASE("dribble-and-turn establishes possession") {
    MatchEngine eng;
    eng.Reset(0xB50000AAu);
    eng.Step(MatchInput{});
    MatchState s = eng.State();
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 9;
    s.sides[0].control.ball_can_be_controlled = 1;
    PlacePlayersAtKickoff(s);
    // Ball at feet — capture without sprinting away from a slow dribble ball.
    s.ball.pos = s.players[9].pos;
    s.ball.pos.z = Fix{};
    s.ball.speed = 0;
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.clock.stoppage_event_timer = 0;
    s.phase = MatchPhase::InPlay;
    eng.LoadState(s);

    MatchInput in{};
    in.p1.dir = Dir::None;
    for (int i = 0; i < 4; ++i) eng.Step(in);
    CHECK(eng.State().sides[0].control.player_has_ball == 1);
    CHECK(eng.State().clock.last_team_played == 1);
}

TEST_CASE("scripted dribble-and-turn hash is stable") {
    const uint64_t a = RunDribbleTurn();
    CHECK(a == RunDribbleTurn());
    constexpr uint64_t kExpected = 0x67fa5be98624d44eull;
    CAPTURE(a);
    CHECK(a == kExpected);
}
