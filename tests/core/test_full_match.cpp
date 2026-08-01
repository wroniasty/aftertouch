// B2 acceptance: 90 simulated minutes headless → FullTime, score 0–0.
#include <doctest/doctest.h>

#include "core/match_clock.hpp"
#include "core/match_engine.hpp"

using namespace at;

TEST_CASE("full match game_length=0 ends 0-0") {
    MatchEngine eng;
    eng.Reset(0xB2000001u);

    // Hard cap: kickoff + 2×(45′ + injury 50) + HT 100 + slack.
    constexpr uint32_t kCap = 20000;
    uint32_t steps = 0;
    while (eng.State().phase != MatchPhase::FullTime && steps < kCap) {
        eng.Step(MatchInput{});
        ++steps;
    }

    CAPTURE(steps);
    CAPTURE(static_cast<int>(eng.State().phase));
    CAPTURE(eng.State().clock.displayed_minute);
    CAPTURE(eng.State().clock.period);
    CAPTURE(eng.State().clock.end_game_counter);

    REQUIRE(eng.State().phase == MatchPhase::FullTime);
    CHECK(eng.State().score[0] == 0);
    CHECK(eng.State().score[1] == 0);
    CHECK(GetGameState(eng.State()) == GameState::GameEnded);
    // At least one half of InProgress clock time.
    CHECK(eng.State().tick >= TicksForNinetyMinutes(0));
}
