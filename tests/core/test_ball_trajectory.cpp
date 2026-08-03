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
    // Re-pinned B13 / R6 (pass fixes): pass targeting is the Amiga's — no range
    // limit, cone anchored at the ball at +-22.5 degrees, aim ray extended past the
    // receiver — and the post-kick lockout no longer blocks team-mates from
    // receiving. Pass strength and the Passing bonus are now the sourced tables.
    // Re-pinned B13 / R7 (goalkeeper): the keeper's resting destination is the
    // Amiga arc-and-band map — a 103px arc across his goal and a 27px depth
    // band — instead of the midpoint between the ball and his own goal line on
    // a 254px arc, which sent him to the halfway line whenever the ball was at
    // the far end. Both keeper callers now share one formula.
    constexpr uint64_t kExpected = 0x9ef1f7e2346ab6b0ull;
    CHECK(a == kExpected);
}
