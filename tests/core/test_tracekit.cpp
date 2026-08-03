// A6: tracekit generate + diff (no golden file — pure synthetic).
#include <doctest/doctest.h>

#include "tracekit/tracekit.hpp"

using namespace at;
using namespace at::tracekit;

TEST_CASE("tracekit self-diff of a generated scenario is identical") {
    const Scenario s = KickoffScenario(50);
    std::vector<uint8_t> a, b;
    REQUIRE(Generate(s, a));
    REQUIRE(Generate(s, b));
    const DiffResult r = Diff(a, b);
    CHECK(r.identical);
    CHECK(a.size() == trace::kHeaderSize + 50 * trace::kRecordSize);
}

TEST_CASE("tracekit reports divergence when a record is perturbed") {
    const Scenario s = KickoffScenario(30);
    std::vector<uint8_t> a, b;
    REQUIRE(Generate(s, a));
    b = a;
    // Flip one payload byte in tick index 7 (not the trailing hash — corrupt hash
    // would also fail decode; we want a content mismatch Diff can name).
    const size_t off = trace::kHeaderSize + 7 * trace::kRecordSize;
    b[off + 4] ^= 0x01;   // phase / early field
    // Recompute would be needed for a valid record; Diff compares hashes first,
    // so a raw flip is enough to report divergence.
    const DiffResult r = Diff(a, b);
    CHECK_FALSE(r.identical);
    CHECK(r.reason != nullptr);
}

TEST_CASE("tracekit rejects truncated traces") {
    const Scenario s = KickoffScenario(10);
    std::vector<uint8_t> a;
    REQUIRE(Generate(s, a));
    a.resize(a.size() / 2);
    const DiffResult r = Diff(a, a);
    CHECK_FALSE(r.identical);
}

TEST_CASE("the shot_curl corpus scenario actually strikes and curls") {
    // A3 §2.7 names this entry for aftertouch. It used to script fire+NE while
    // every home player was ~200 units from the ball, so it recorded a walk and
    // no kick — and a corpus entry that never reaches its mechanic cannot
    // report a divergence in it (B6a / Track I).
    const Scenario sc = ShotCurlScenario(80);
    MatchEngine eng;
    eng.Reset(sc.seed);
    const SetupFn setup = SetupFnFor(sc.setup);
    REQUIRE(setup != nullptr);
    eng.Step(MatchInput{});
    MatchState s0 = eng.State();
    setup(s0);
    eng.LoadState(s0);

    const int16_t start_x = eng.State().ball.pos.x.Whole();
    const int16_t start_y = eng.State().ball.pos.y.Whole();
    bool struck = false;
    bool latched = false;
    int16_t prev_spin = eng.State().sides[0].control.spin_timer;
    for (const MatchInput& in : sc.inputs) {
        eng.Step(in);
        const at::TeamControl& tc = eng.State().sides[0].control;
        if (prev_spin == kSpinInactive && tc.spin_timer != kSpinInactive) struck = true;
        if (tc.spin_cw || tc.spin_ccw) latched = true;
        prev_spin = tc.spin_timer;
    }
    CHECK(struck);
    CHECK(latched);
    const int dx = eng.State().ball.pos.x.Whole() - start_x;
    const int dy = eng.State().ball.pos.y.Whole() - start_y;
    CHECK(dx * dx + dy * dy > 100 * 100); // the ball actually went somewhere
}
