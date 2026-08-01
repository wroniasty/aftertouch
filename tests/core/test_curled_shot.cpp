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
    GiveBallForTest(s, 0, 9);
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.clock.stoppage_event_timer = 0;
    s.phase = MatchPhase::InPlay;
    eng.LoadState(s);

    MatchInput in{};
    in.p1.dir = Dir::N;
    in.p1.fire = true;
    // Hold past kFireHoldThreshold (every tick) + one team-switch for dispatch.
    for (int i = 0; i < kFireHoldThreshold + 4; ++i) eng.Step(in);

    CHECK(eng.State().sides[0].control.player_has_ball == 0);
    CHECK(eng.State().sides[0].control.pass_kick_timer > 0);
    CHECK(eng.State().ball.speed > 0);
}

TEST_CASE("scripted curled shot hash is stable") {
    const uint64_t a = RunCurledShot();
    CHECK(a == RunCurledShot());
    constexpr uint64_t kExpected = 0x7beee033074190edull;
    CAPTURE(a);
    CHECK(a == kExpected);
}
