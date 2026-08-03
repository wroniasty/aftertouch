// B6a / S6: aftertouch lofts a pass (AFTERTOUCH §6).
//
// long_pass / long_spin_pass are the reference's pass-loft gate. They were
// dead in the kick path — written as 0 at launch and never set — while the
// field itself was reused as the B8 restart-shortfall counter, so a chipped
// pass was unreachable. The two uses are now separate fields.
#include <doctest/doctest.h>

#include "core/aftertouch.hpp"
#include "core/set_pieces.hpp"
#include "core/shooting.hpp"
#include "kick_fixture.hpp"

using namespace at;
using namespace at::test;

namespace {

KickRun RunPass(Dir kick, Dir after, int react_delay = 1) {
    MatchEngine eng = MakeKickEngine(336, 449);
    KickScript sc;
    sc.kick_dir      = kick;
    sc.after_dir     = after;
    sc.react_delay   = react_delay;
    sc.dribble_ticks = 20;
    sc.hold_ticks    = 20 + kFireHoldThreshold - 2; // released: a tap, so a pass
    sc.total_ticks   = 60;
    // hold_ticks counts from the first Step of RunKick, after the dribble.
    sc.hold_ticks    = kFireHoldThreshold - 2;
    return RunKick(eng, sc);
}

} // namespace

TEST_CASE("a pass stays on the deck when the stick is left alone") {
    const KickRun r = RunPass(Dir::N, Dir::N);
    REQUIRE(r.struck);
    REQUIRE(r.was_pass);
    CHECK(r.peak_z == Fix{});
}

TEST_CASE("pushing back against a pass lofts it") {
    const KickRun r = RunPass(Dir::N, Dir::S); // diff 4: straight back
    REQUIRE(r.struck);
    REQUIRE(r.was_pass);
    CHECK(r.peak_z > Fix::FromInt(2));
}

TEST_CASE("a lofted pass is flagged, a ground pass is not") {
    MatchEngine eng = MakeKickEngine(336, 449);
    KickScript sc;
    sc.kick_dir      = Dir::N;
    sc.after_dir     = Dir::S;
    sc.react_delay   = 1;
    sc.dribble_ticks = 20;
    sc.hold_ticks    = kFireHoldThreshold - 2;
    sc.total_ticks   = 40;
    const KickRun r = RunKick(eng, sc);
    REQUIRE(r.struck);
    REQUIRE(r.was_pass);
    CHECK(eng.State().sides[0].control.long_pass == 1);
    // The restart-shortfall counter is a different field and must be untouched.
    CHECK(eng.State().sides[0].control.restart_shortfall == 0);
}

TEST_CASE("a curled ground pass does not set the loft flag") {
    MatchEngine eng = MakeKickEngine(336, 449);
    KickScript sc;
    sc.kick_dir      = Dir::N;
    sc.after_dir     = Dir::E; // perpendicular: curl, no loft
    sc.react_delay   = 1;
    sc.dribble_ticks = 20;
    sc.hold_ticks    = kFireHoldThreshold - 2;
    sc.total_ticks   = 40;
    const KickRun r = RunKick(eng, sc);
    REQUIRE(r.struck);
    REQUIRE(r.was_pass);
    CHECK(eng.State().sides[0].control.long_pass == 0);
    CHECK(r.peak_z == Fix{});
}
