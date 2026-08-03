// B6a / S4: the carrier must still have the ball at the end of a fire charge.
//
// Suppressing the dribble while fire is held leaves the ball behind a running
// carrier; possession drops at the close band and the queued strike arrives as
// a slide or a header instead of a kick.
#include <doctest/doctest.h>

#include "core/shooting.hpp"
#include "kick_fixture.hpp"

using namespace at;
using namespace at::test;

TEST_CASE("a charge keeps the ball: every direction, moving and standing") {
    for (int d = 0; d < 8; ++d) {
        for (int dribble : {0, 25}) {
            const Dir dir = static_cast<Dir>(d);
            CAPTURE(d);
            CAPTURE(dribble);

            MatchEngine eng = MakeKickEngine(336, 449);
            KickScript sc;
            sc.kick_dir      = dir;
            sc.after_dir     = dir;
            sc.react_delay   = 1;
            sc.dribble_ticks = dribble;
            sc.total_ticks   = kFireHoldThreshold + 12;
            const KickRun r = RunKick(eng, sc);

            CHECK(r.struck);       // the charge produced a kick
            CHECK_FALSE(r.contested);  // not a slide or a header
            CHECK_FALSE(r.lost_ball);  // possession held all the way to the strike
            CHECK_FALSE(r.was_pass);
        }
    }
}

TEST_CASE("a charge from a ball parked at the feet still shoots") {
    // The state after receiving a pass: the ball sits at rest on the receiver's
    // foot (possession.hpp parks it). Running while charging used to walk the
    // carrier straight off it.
    MatchEngine eng = MakeKickEngine(336, 600);
    KickScript sc;
    sc.kick_dir      = Dir::N;
    sc.after_dir     = Dir::N;
    sc.dribble_ticks = 0;
    sc.total_ticks   = kFireHoldThreshold + 12;
    const KickRun r = RunKick(eng, sc);

    CHECK(r.struck);
    CHECK_FALSE(r.contested);
    CHECK(r.launch_speed > 0);
}

TEST_CASE("the carrier does not outrun the ball while charging") {
    MatchEngine eng = MakeKickEngine(336, 600);
    MatchInput in{};
    in.p1.dir = Dir::N;
    for (int t = 0; t < 20; ++t) { in.p1.fire = false; eng.Step(in); }

    int32_t worst = 0;
    for (int t = 0; t < kFireHoldThreshold - 1; ++t) {
        in.p1.fire = true;
        eng.Step(in);
        const MatchState& s = eng.State();
        const int32_t d2 = PossessionBallDistSq(s.players[kStrikerSlot], s.ball);
        if (d2 > worst) worst = d2;
        CHECK(s.sides[0].control.player_has_ball == 1);
    }
    CAPTURE(worst);
    CHECK(worst <= kDistCloseSq);
}
