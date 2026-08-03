// B6 acceptance: scripted hold-shot + off-axis aftertouch → HashState pin.
#include <doctest/doctest.h>

#include "core/hash.hpp"
#include "core/match_engine.hpp"
#include "core/match_input.hpp"
#include "core/movement.hpp"
#include "core/possession.hpp"
#include "core/shooting.hpp"

using namespace at;

namespace {

uint64_t RunCurledShot() {
    MatchEngine eng;
    eng.Reset(0xB6000001u);
    eng.Step(MatchInput{});

    MatchState s = eng.State();
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 9;
    s.sides[0].control.ball_can_be_controlled = 1;
    s.globals.team_playing_up = 1;
    for (int r = 0; r < kMatchTacticRoles; ++r)
        for (int q = 0; q < kMatchBallQuadrants; ++q)
            s.sides[0].tactics.cells[static_cast<size_t>(r)][static_cast<size_t>(q)] =
                static_cast<uint8_t>(((r % 15) << 4) | (q % 16));
    s.sides[0].squad[9].attrs.speed = 5;
    s.sides[0].squad[9].attrs.shooting = 4;
    s.sides[0].squad[9].attrs.finishing = 4;
    s.sides[0].squad[9].attrs.ball_control = 4;

    PlacePlayersAtKickoff(s);
    // Stand the striker up the pitch with room to run: the tactics grid above
    // parks him near the byline, where a charge now dribbles the ball straight
    // out of play (it only stayed put while the fire hold froze the dribble).
    s.players[9].pos.x = Fix::FromInt(336);
    s.players[9].pos.y = Fix::FromInt(600);
    s.players[9].dest_x = 336;
    s.players[9].dest_y = 600;
    GiveBallForTest(s, 0, 9);
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.clock.stoppage_event_timer = 0;
    s.phase = MatchPhase::InPlay;
    eng.LoadState(s);

    MatchInput in{};
    // Hold fire facing north until shot, then curl NE.
    for (int i = 0; i < kFireHoldThreshold + 4; ++i) {
        in.p1.dir = Dir::N;
        in.p1.fire = true;
        eng.Step(in);
    }
    for (int i = 0; i < 40; ++i) {
        in.p1.dir = Dir::NE;
        in.p1.fire = false;
        eng.Step(in);
    }
    return HashState(eng.State());
}

} // namespace

TEST_CASE("hold fire near ball launches a shot") {
    MatchEngine eng;
    eng.Reset(0xB60000AAu);
    eng.Step(MatchInput{});
    MatchState s = eng.State();
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 9;
    PlacePlayersAtKickoff(s);
    s.players[9].pos.x = Fix::FromInt(336);
    s.players[9].pos.y = Fix::FromInt(600);
    s.players[9].dest_x = 336;
    s.players[9].dest_y = 600;
    GiveBallForTest(s, 0, 9);
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.clock.stoppage_event_timer = 0;
    s.phase = MatchPhase::InPlay;
    eng.LoadState(s);

    MatchInput in{};
    in.p1.dir = Dir::N;
    in.p1.fire = true;
    // Assert on the strike itself: once the ball is loose, anyone may touch it,
    // so reading possession N ticks later measures the rest of the sim too.
    // kSpinArmed does not survive the Step it is set on — UpdateBall opens the
    // window in the same tick — so watch for the transition out of inactive.
    int strike = -1;
    int16_t prev = eng.State().sides[0].control.spin_timer;
    for (int i = 0; i < kFireHoldThreshold + 4 && strike < 0; ++i) {
        eng.Step(in);
        const int16_t now = eng.State().sides[0].control.spin_timer;
        if (prev == kSpinInactive && now != kSpinInactive) strike = i;
        prev = now;
    }

    REQUIRE(strike >= 0);
    CHECK(strike >= kFireHoldThreshold - 1); // a hold, not a tap
    CHECK(eng.State().sides[0].control.player_has_ball == 0);
    CHECK(eng.State().sides[0].control.pass_in_progress == 0);
    CHECK(eng.State().sides[0].control.pass_kick_timer > 0);
    CHECK(eng.State().clock.last_team_played == 1);
}

TEST_CASE("scripted curled shot hash is stable") {
    const uint64_t a = RunCurledShot();
    CHECK(a == RunCurledShot());
    // Re-pinned B13 / R3: the curl table's magnitudes are the Amiga's (12 → 32 on a
    // cardinal axis, 8 → 23 on a diagonal) and the decay ramp is the Amiga's. Net
    // curl authority is ~1.6× what it was and arrives in a much shorter burst. This
    // is the scenario the whole project is named after — the hash moving here is the
    // headline of R3, not a side effect of it.
    //
    // Previously (B6a): scenario moved up the pitch and the curl bends with the stick
    // from spin_timer 0 (S1/S2).
    // Re-pinned B13 / R6 (pass fixes): pass targeting is the Amiga's — no range
    // limit, cone anchored at the ball at +-22.5 degrees, aim ray extended past the
    // receiver — and the post-kick lockout no longer blocks team-mates from
    // receiving. Pass strength and the Passing bonus are now the sourced tables.
    // Re-pinned B13 / R7 (goalkeeper): the keeper's resting destination is the
    // Amiga arc-and-band map — a 103px arc across his goal and a 27px depth
    // band — instead of the midpoint between the ball and his own goal line on
    // a 254px arc, which sent him to the halfway line whenever the ball was at
    // the far end. Both keeper callers now share one formula.
    constexpr uint64_t kExpected = 0x93c00bbe9f40b909ull;
    CAPTURE(a);
    CHECK(a == kExpected);
}
