// B6a / S3: fire hold semantics through the Step loop.
//
// normalFire in the reference is a flag the on-ball dispatch re-reads every
// frame (SHOOTING §1). Firing it as a one-tick pulse at exactly the threshold
// means a hold that is not strikeable on that single tick is swallowed and
// never re-arms while the button stays down.
#include <doctest/doctest.h>

#include "core/aftertouch.hpp"
#include "core/shooting.hpp"
#include "kick_fixture.hpp"

using namespace at;
using namespace at::test;

namespace {

struct HoldOutcome {
    int  strikes = 0;
    int  first_strike_tick = -1;
    bool was_pass = false;
};

// Holds fire for `hold` Steps with `dir` on the stick from `dir_from` onward,
// then keeps running to see how many launches happened in total.
HoldOutcome RunHold(MatchEngine& eng, int hold, Dir dir, int dir_from, int total) {
    HoldOutcome out;
    MatchInput in{};
    int prev_spin = eng.State().sides[0].control.spin_timer;
    for (int t = 0; t < total; ++t) {
        in.p1.fire = (t < hold);
        in.p1.dir  = (t >= dir_from) ? dir : Dir::None;
        eng.Step(in);
        const TeamControl& tc = eng.State().sides[0].control;
        if (prev_spin == kSpinInactive && tc.spin_timer != kSpinInactive) {
            ++out.strikes;
            if (out.first_strike_tick < 0) {
                out.first_strike_tick = t;
                out.was_pass = tc.pass_in_progress != 0;
            }
        }
        prev_spin = tc.spin_timer;
    }
    return out;
}

} // namespace

TEST_CASE("a tap passes and a hold shoots") {
    {
        MatchEngine eng = MakeKickEngine(336, 600);
        const HoldOutcome o = RunHold(eng, kFireHoldThreshold - 1, Dir::N, 0, 40);
        CHECK(o.strikes == 1);
        CHECK(o.was_pass);
    }
    {
        MatchEngine eng = MakeKickEngine(336, 600);
        const HoldOutcome o = RunHold(eng, kFireHoldThreshold + 6, Dir::N, 0, 40);
        CHECK(o.strikes == 1);
        CHECK_FALSE(o.was_pass);
    }
}

TEST_CASE("a hold that crosses the threshold while unable to strike still fires") {
    // Stick neutral for the whole charge: no kick direction, so the strike is
    // refused on the threshold tick. Pressing a direction while still holding
    // must produce the shot rather than requiring a release and a fresh hold.
    MatchEngine eng = MakeKickEngine(336, 600);
    const int dir_from = kFireHoldThreshold + 6;
    const HoldOutcome o = RunHold(eng, dir_from + 20, Dir::N, dir_from, 60);

    CHECK(o.strikes == 1);
    CHECK_FALSE(o.was_pass);
    CHECK(o.first_strike_tick >= dir_from);
    // Two ticks of team alternation is the whole budget for the dispatch.
    CHECK(o.first_strike_tick <= dir_from + 2);
}

TEST_CASE("holding fire through a strike does not strike twice") {
    MatchEngine eng = MakeKickEngine(336, 600);
    const HoldOutcome o = RunHold(eng, 80, Dir::N, 0, 80);
    CHECK(o.strikes == 1);
}

TEST_CASE("the hold threshold is the only tap/hold boundary") {
    for (int hold = 1; hold <= kFireHoldThreshold + 3; ++hold) {
        CAPTURE(hold);
        MatchEngine eng = MakeKickEngine(336, 600);
        const HoldOutcome o = RunHold(eng, hold, Dir::N, 0, 40);
        REQUIRE(o.strikes == 1);
        CHECK(o.was_pass == (hold < kFireHoldThreshold));
    }
}
