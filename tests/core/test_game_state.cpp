// B2: kickoff / half-time / full-time state machine.
#include <doctest/doctest.h>

#include "core/match_clock.hpp"
#include "core/match_engine.hpp"

using namespace at;

TEST_CASE("forced half-time transition swaps ends and resumes") {
    MatchState s{};
    s.clock.match_started = 1;
    s.clock.period = 0;
    s.globals.team_playing_up = 1;
    s.globals.team_starting = 2;
    SetPl(s, GameStatePl::InProgress);
    EndFirstHalf(s);

    CHECK(s.phase == MatchPhase::HalfTime);
    CHECK(GetGameState(s) == GameState::FirstHalfEnded);
    CHECK(s.globals.team_playing_up == 2);
    CHECK(s.globals.team_starting == 1);
    CHECK(s.clock.stoppage_event_timer == 100);

    // Drain stoppage.
    for (int i = 0; i < 100; ++i) UpdateTime(s);
    CHECK(s.phase == MatchPhase::InPlay);
    CHECK(s.clock.period == 1);
    CHECK(GetPl(s) == GameStatePl::InProgress);
}

TEST_CASE("EndSecondHalf sets FullTime") {
    MatchState s{};
    EndSecondHalf(s);
    CHECK(s.phase == MatchPhase::FullTime);
    CHECK(GetGameState(s) == GameState::GameEnded);
    CHECK(s.score[0] == 0);
    CHECK(s.score[1] == 0);
}
