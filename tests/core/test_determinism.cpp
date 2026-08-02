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
    constexpr uint64_t kExpected = 0x508ba88280048dacull;
    CHECK(got == kExpected);
}
