// B2: fixed-step Amiga-profile match clock.
#include <doctest/doctest.h>

#include "core/match_clock.hpp"
#include "core/match_engine.hpp"

using namespace at;

TEST_CASE("TicksForNinetyMinutes matches Amiga length-0 formula") {
    CHECK(TicksForNinetyMinutes(0) == 8820u);
    CHECK(ClockRefill() == 49);
}

TEST_CASE("clock advances game-seconds only while InProgress") {
    MatchState s{};
    s.clock.match_started = 1;
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.phase = MatchPhase::InPlay;

    const int16_t before_min = s.clock.displayed_minute;
    for (int i = 0; i < 200; ++i) UpdateTime(s);
    CHECK(s.clock.displayed_minute >= before_min);
    // With delta 30 / refill 49, 200 ticks produce some seconds.
    CHECK(s.clock.displayed_minute * 60 + (s.clock.game_seconds > 0 ? s.clock.game_seconds : 0) >
          0);

    MatchState stopped = s;
    SetPl(stopped, GameStatePl::Stopped);
    const auto min = stopped.clock.displayed_minute;
    const auto sec = stopped.clock.game_seconds;
    const auto acc = stopped.clock.seconds_accumulator;
    for (int i = 0; i < 200; ++i) UpdateTime(stopped);
    CHECK(stopped.clock.displayed_minute == min);
    CHECK(stopped.clock.game_seconds == sec);
    CHECK(stopped.clock.seconds_accumulator == acc);
}

TEST_CASE("engine kickoff reaches InPlay within a few ticks") {
    MatchEngine eng;
    eng.Reset(1);
    CHECK(eng.State().phase == MatchPhase::KickOff);
    for (int i = 0; i < 5; ++i) eng.Step(MatchInput{});
    CHECK(eng.State().phase == MatchPhase::InPlay);
    CHECK(GetPl(eng.State()) == GameStatePl::InProgress);
    CHECK(eng.State().ball.pos.x.Whole() == kCentreSpotX);
    CHECK(eng.State().ball.pos.y.Whole() == kCentreSpotY);
}
