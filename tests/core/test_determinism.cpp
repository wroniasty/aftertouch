#include <doctest/doctest.h>

#include "core/hash.hpp"
#include "core/match_engine.hpp"

using namespace at;

namespace {

uint64_t RunScenario() {
    MatchEngine eng;
    eng.Reset(0xA2DEu);

    MatchInput in{};
    for (int i = 0; i < 200; ++i) {
        in.p1.dir  = static_cast<Dir>(i % 8);
        in.p1.fire = (i % 5) == 0;
        in.p2.dir  = Dir::None;
        eng.Step(in);
    }
    return HashState(eng.State());
}

} // namespace

TEST_CASE("determinism gate: committed hash of a scripted scenario") {
    // Same seed + same inputs ⇒ bit-identical HashState on every platform.
    const uint64_t got = RunScenario();
    CHECK(got == RunScenario());

    // Pinned after B4 player movement. If a hashed field changes on purpose,
    // print got and update this literal.
    // Re-pinned B13 / R6 (pass fixes): pass targeting is the Amiga's — no range
    // limit, cone anchored at the ball at +-22.5 degrees, aim ray extended past the
    // receiver — and the post-kick lockout no longer blocks team-mates from
    // receiving. Pass strength and the Passing bonus are now the sourced tables.
    // Re-pinned B13 / R7 (goalkeeper): the keeper's resting destination is the
    // Amiga arc-and-band map — a 103px arc across his goal and a 27px depth
    // band — instead of the midpoint between the ball and his own goal line on
    // a 254px arc, which sent him to the halfway line whenever the ball was at
    // the far end. Both keeper callers now share one formula.
    constexpr uint64_t kExpected = 0x59d3316aea3c1071ull;
    CHECK(got == kExpected);
}
