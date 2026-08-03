// B6a / Track I: the control telemetry the C1a HUD and the transcript report.
// A "shooting feels off" report should arrive as a tick count, so the thing
// producing the tick count needs a test of its own.
#include <doctest/doctest.h>

#include "tracekit/tracekit.hpp"

#include "kick_fixture.hpp"

using namespace at;
using namespace at::tracekit;
using at::test::MakeKickEngine;
using at::test::kStrikerSlot;

namespace {

// Drives a charge and returns the probe's view of it.
KickTelemetry ProbeKick(Dir kick, Dir after, int react_delay, int hold,
                        int dribble = 20, int total = 45) {
    MatchEngine eng = MakeKickEngine(336, 560);
    KickProbe probe;
    MatchInput in{};
    in.p1.dir = kick;
    for (int t = 0; t < dribble; ++t) {
        in.p1.fire = false;
        eng.Step(in);
        probe.Observe(eng.State(), in);
    }
    int strike = -1;
    for (int t = 0; t < total; ++t) {
        in.p1.fire = (t < hold);
        in.p1.dir  = (strike >= 0 && (t - strike) >= react_delay) ? after : kick;
        eng.Step(in);
        probe.Observe(eng.State(), in);
        const KickTelemetry* k = probe.Last(0);
        if (strike < 0 && k) strike = t;
    }
    const KickTelemetry* k = probe.Last(0);
    return k ? *k : KickTelemetry{};
}

} // namespace

TEST_CASE("the probe reports press-to-strike latency for a hold") {
    const KickTelemetry k = ProbeKick(Dir::N, Dir::N, 1, 40);
    CHECK(k.strike_tick > 0);
    CHECK_FALSE(k.was_pass);
    CHECK(k.press_to_strike >= kFireHoldThreshold - 1);
    // One team-alternation tick of dispatch slack on top of the threshold.
    CHECK(k.press_to_strike <= kFireHoldThreshold + 2);
    CHECK(k.hold_ticks >= kFireHoldThreshold);
}

TEST_CASE("the probe reports a tap as a pass with a shorter latency") {
    const KickTelemetry k = ProbeKick(Dir::N, Dir::N, 1, kFireHoldThreshold - 3);
    CHECK(k.was_pass);
    CHECK(k.press_to_strike < kFireHoldThreshold + 2);
}

TEST_CASE("the probe reports where the curl latched and on which side") {
    const KickTelemetry cw = ProbeKick(Dir::N, Dir::NE, 1, 40);
    CHECK(cw.latch_spin == 0);
    CHECK(cw.latch_side == 1);

    const KickTelemetry ccw = ProbeKick(Dir::N, Dir::NW, 1, 40);
    CHECK(ccw.latch_spin == 0);
    CHECK(ccw.latch_side == -1);

    const KickTelemetry late = ProbeKick(Dir::N, Dir::NE, 4, 40);
    CHECK(late.latch_spin == 3);
}

TEST_CASE("the probe names the vertical decision") {
    CHECK(ProbeKick(Dir::N, Dir::E, 1, 40).vertical == VerticalDecision::Drive);
    CHECK(ProbeKick(Dir::N, Dir::S, 1, 40).vertical == VerticalDecision::Lob);
    CHECK(ProbeKick(Dir::N, Dir::N, 1, 40).vertical == VerticalDecision::None);
}

TEST_CASE("the probe flags a lofted pass") {
    const KickTelemetry k = ProbeKick(Dir::N, Dir::S, 1, kFireHoldThreshold - 3);
    REQUIRE(k.was_pass);
    CHECK(k.lofted_pass);
    CHECK_FALSE(ProbeKick(Dir::N, Dir::E, 1, kFireHoldThreshold - 3).lofted_pass);
}

TEST_CASE("the transcript records a kick line with the latency") {
    const Scenario sc = ShotCurlScenario(60);
    std::string text;
    REQUIRE(WriteSparseTranscript(sc, text));
    // The line gained dir= / target= / aim= fields, so assert the fields rather
    // than their adjacency — the aim point is what answers "the pass went where?"
    const size_t kick = text.find("kick: side=1 shot");
    REQUIRE(kick != std::string::npos);
    const std::string line = text.substr(kick, text.find('\n', kick) - kick);
    CHECK(line.find("press->strike=") != std::string::npos);
    CHECK(line.find("aim=(") != std::string::npos);
}
