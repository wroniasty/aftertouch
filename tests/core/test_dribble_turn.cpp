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
    // Re-pinned B13 / R4: the dribble touch-count is live. A turn now costs a
    // touch, and past the carrier's Ball-Control threshold the ball gets away
    // from him — the mechanic CONTROL.md §4 was missing entirely, and the reason
    // this scenario in particular had to move.
    //
    // Previously (B6a): the dribble is no longer frozen while fire is held (S4).
    // Re-pinned B13 / R6 (pass fixes): pass targeting is the Amiga's — no range
    // limit, cone anchored at the ball at +-22.5 degrees, aim ray extended past the
    // receiver — and the post-kick lockout no longer blocks team-mates from
    // receiving. Pass strength and the Passing bonus are now the sourced tables.
    // Re-pinned B13 / R7 (goalkeeper): the keeper's resting destination is the
    // Amiga arc-and-band map — a 103px arc across his goal and a 27px depth
    // band — instead of the midpoint between the ball and his own goal line on
    // a 254px arc, which sent him to the halfway line whenever the ball was at
    // the far end. Both keeper callers now share one formula.
    // Re-pinned B13 / R8 (restart placement): a restart now puts the ball on its
    // spot instead of where it crossed the line — goal kicks at the six-yard box
    // (396/276, y 154/744), corners at the flag, throw-ins pinned to the
    // touchline — and the keeper takes his own goal kick.
    constexpr uint64_t kExpected = 0x2284975e6e1f1c11ull;
    CAPTURE(a);
    CHECK(a == kExpected);
}
