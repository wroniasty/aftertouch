// B3 acceptance: scripted kick produces a committed HashState pin.
#include <doctest/doctest.h>

#include "core/ball.hpp"
#include "core/hash.hpp"
#include "core/match_engine.hpp"

using namespace at;

namespace {

uint64_t RunKickScenario() {
    MatchEngine eng;
    eng.Reset(0xB3DEAD01u);

    for (int i = 0; i < 8; ++i) eng.Step(MatchInput{});

    MatchState s = eng.State();
    // Short lofted chip toward the top half — stays in play for the pin window.
    LaunchBall(s, Dest{kCentreSpotX, static_cast<int16_t>(kCentreSpotY - 80)},
               2048, Fix::FromRaw(30000));
    s.clock.last_team_played = 1;
    eng.LoadState(s);

    for (int i = 0; i < 120; ++i) eng.Step(MatchInput{});
    return HashState(eng.State());
}

} // namespace

TEST_CASE("scripted kick trajectory hash is stable") {
    const uint64_t a = RunKickScenario();
    CHECK(a == RunKickScenario());

    // Pinned after B3 UpdateBall. If physics change on purpose, print and update.
    // Pinned after B3 UpdateBall. If physics change on purpose, print and update.
    constexpr uint64_t kExpected = 0x81369f8d9a66269dull;
    CHECK(a == kExpected);
}
