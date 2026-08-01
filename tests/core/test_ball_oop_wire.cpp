// B3: OOP classification wired into UpdateBall / Step.
#include <doctest/doctest.h>

#include "core/ball.hpp"
#include "core/match_engine.hpp"

using namespace at;

TEST_CASE("scripted exit through touchline stops play") {
    MatchEngine eng;
    eng.Reset(0xB3000001u);

    // Drain kick-off stoppage into InProgress.
    for (int i = 0; i < 8; ++i) eng.Step(MatchInput{});
    REQUIRE(GetPl(eng.State()) == GameStatePl::InProgress);

    MatchState s = eng.State();
    LaunchBall(s, Dest{0, kCentreSpotY}, 4000, Fix{});
    s.clock.last_team_played = 1;
    eng.LoadState(s);

    bool stopped = false;
    for (int i = 0; i < 200; ++i) {
        eng.Step(MatchInput{});
        if (GetPl(eng.State()) == GameStatePl::Stopped) {
            stopped = true;
            break;
        }
    }
    REQUIRE(stopped);
    const GameState gs = GetGameState(eng.State());
    CHECK((gs == GameState::ThrowInForwardLeft ||
           gs == GameState::ThrowInCentreLeft ||
           gs == GameState::ThrowInBackLeft));
    CHECK(eng.State().globals.foul_x < kPlayableMinX);
}
