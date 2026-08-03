// B13 / R4 — the dribble touch-count, the goal/save resolution stage, the
// goalmouth scatter and the near-miss whistle flag.
//
// Asserted as properties rather than table values wherever possible, per
// B6a §3 rule 2: the day these are fitted against a trace is the day the tests
// must not need editing.
#include <doctest/doctest.h>

#include "core/aftertouch.hpp"
#include "core/ball.hpp"
#include "core/goalkeeper.hpp"
#include "core/match_engine.hpp"
#include "core/movement.hpp"
#include "core/possession.hpp"
#include "core/set_pieces.hpp"
#include "core/profile.hpp"

using namespace at;

namespace {

// A carrier on the centre spot with the ball at his feet, free to turn.
MatchState MakeCarrier(uint8_t control_attr) {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.globals.team_playing_up = 1;
    TeamControl& tc = s.sides[0].control;
    tc.player_number = 1;
    tc.team_number = 1;
    tc.controlled_slot = 0;
    tc.ball_in_play = 1;

    Entity& p = s.players[0];
    p.team_number = 1;
    p.player_ordinal = 1;
    p.player_state = static_cast<uint8_t>(PlayerState::Normal);
    p.pos.x = Fix::FromInt(kCentreSpotX);
    p.pos.y = Fix::FromInt(kCentreSpotY);
    p.speed = 1000;
    s.sides[0].squad[0].attrs.ball_control = control_attr;

    GiveBallForTest(s, 0, 0);
    return s;
}

// Turn the carrier through alternating octants; return how many turns he made
// before possession was interrupted (or -1 if it never was).
int TurnsUntilLoss(uint8_t control_attr, int max_turns) {
    MatchState s = MakeCarrier(control_attr);
    TeamControl& tc = s.sides[0].control;
    for (int i = 0; i < max_turns; ++i) {
        tc.current_allowed_direction = static_cast<int16_t>(i & 7);
        tc.pl_very_close_to_ball = 1;
        tc.pl_close_to_ball = 1;
        ApplyDribble(s, 0);
        if (!tc.player_has_ball) return i;
    }
    return -1;
}

} // namespace

TEST_CASE("a dribbler loses the ball after a Control-derived number of turns") {
    for (uint8_t c = 0; c <= kAttrMax; ++c) {
        CAPTURE(c);
        const int turns = TurnsUntilLoss(c, 200);
        REQUIRE(turns > 0); // it must actually happen
        CHECK(turns == kDribbleTouchesAllowed[c] + 1);
    }
}

TEST_CASE("better Control buys strictly more turns") {
    int prev = TurnsUntilLoss(0, 200);
    for (uint8_t c = 1; c <= kAttrMax; ++c) {
        const int here = TurnsUntilLoss(c, 200);
        CAPTURE(c);
        CHECK(here > prev);
        prev = here;
    }
    // The accelerating spacing is the point: the top of the scale is worth much
    // more than the bottom.
    const int low = TurnsUntilLoss(1, 200) - TurnsUntilLoss(0, 200);
    const int high = TurnsUntilLoss(7, 200) - TurnsUntilLoss(6, 200);
    CHECK(high > low);
}

TEST_CASE("running straight costs no touches") {
    MatchState s = MakeCarrier(0); // worst Control in the game
    TeamControl& tc = s.sides[0].control;
    for (int i = 0; i < 200; ++i) {
        tc.current_allowed_direction = static_cast<int16_t>(Dir::N);
        tc.pl_very_close_to_ball = 1;
        tc.pl_close_to_ball = 1;
        ApplyDribble(s, 0);
    }
    CHECK(tc.player_has_ball == 1);
    CHECK(s.players[0].dribble_touches == 0);
}

TEST_CASE("winning the ball resets the touch budget") {
    MatchState s = MakeCarrier(0);
    TeamControl& tc = s.sides[0].control;
    tc.current_allowed_direction = static_cast<int16_t>(Dir::E);
    tc.pl_very_close_to_ball = 1;
    ApplyDribble(s, 0);
    CHECK(s.players[0].dribble_touches > 0);
    GiveBallForTest(s, 0, 0);
    CHECK(s.players[0].dribble_touches == 0);
}

// --- goal versus save -------------------------------------------------------

TEST_CASE("an evenly matched striker and keeper is exactly 50 50") {
    for (int level = 0; level <= static_cast<int>(kAttrMax); ++level) {
        int goals = 0;
        for (uint32_t tick = 0; tick < 32; ++tick)
            if (ShotBeatsKeeper(level, level, tick)) ++goals;
        CAPTURE(level);
        CHECK(goals == 16); // 16 of 32 ticks — the design signature
    }
}

TEST_CASE("the goal chance is monotonic in the skill difference") {
    const auto rate = [](int fin, int gk) {
        int goals = 0;
        for (uint32_t tick = 0; tick < 32; ++tick)
            if (ShotBeatsKeeper(fin, gk, tick)) ++goals;
        return goals;
    };
    int prev = rate(0, static_cast<int>(kAttrMax)); // worst possible mismatch
    for (int d = -6; d <= 7; ++d) {
        const int fin = d >= 0 ? d : 0;
        const int gk = d >= 0 ? 0 : -d;
        const int here = rate(fin, gk);
        CAPTURE(d);
        CHECK(here >= prev);
        prev = here;
    }
    // Full spread: 6.25 % at -7 through 93.75 % at +7.
    CHECK(rate(0, 7) == 2);  // 1/16 of 32
    CHECK(rate(7, 0) == 30); // 15/16 of 32
}

TEST_CASE("the goal or save roll consumes no RNG") {
    MatchState s{};
    const RngStream before = s.resolve_rng;
    for (uint32_t tick = 0; tick < 64; ++tick)
        (void)ShotBeatsKeeper(4, 3, tick);
    CHECK(s.resolve_rng.seed == before.seed);
    CHECK(s.resolve_rng.xor_key == before.xor_key);
    CHECK(s.resolve_rng.xor_index == before.xor_index);
}

// --- goalmouth scatter ------------------------------------------------------

TEST_CASE("the goalmouth scatter spans -256 to +240 in steps of 16") {
    int16_t lo = 32767, hi = -32768;
    for (uint32_t tick = 0; tick < 32; ++tick) {
        const int16_t v = GoalmouthScatter(tick);
        CHECK(v % 16 == 0);
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    CHECK(lo == -256);
    CHECK(hi == 240);
    // Deterministic in the frame counter, and it cycles every 32 ticks.
    CHECK(GoalmouthScatter(7) == GoalmouthScatter(39));
}

// --- the celebration draw ---------------------------------------------------

namespace {

// Put the ball over the goal line inside the mouth and resolve out-of-play.
MatchState ScoreAGoal() {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.globals.team_playing_up = 1;
    s.clock.last_team_played = 1;
    s.gameplay_rng.Seed(0xA5A50001u);
    s.ball.pos.x = Fix::FromInt(kCentreSpotX);
    s.ball.pos.y = Fix::FromInt(static_cast<int16_t>(kPlayableMinY - 2));
    s.ball.pos.z = Fix{};
    s.ball.speed = 1500;
    WireOutOfPlay(s);
    return s;
}

int DrawsConsumed(const RngStream& before, const RngStream& after) {
    RngStream probe = before;
    for (int n = 0; n <= 8; ++n) {
        if (probe.seed == after.seed && probe.xor_key == after.xor_key &&
            probe.xor_index == after.xor_index)
            return n;
        (void)probe.Draw();
    }
    return -1;
}

} // namespace

// The sourced part of the celebration is its cost to the RNG stream, not its
// length. Two draws per goal, every goal — skip them and every roll after the
// first goal of a match is offset, which no scenario pinned before B13 would
// have caught because none of them scores.
TEST_CASE("a goal consumes exactly two draws from the match stream") {
    MatchState fresh{};
    fresh.gameplay_rng.Seed(0xA5A50001u);
    const RngStream before = fresh.gameplay_rng;

    const MatchState scored = ScoreAGoal();
    REQUIRE(scored.phase == MatchPhase::Goal);
    CHECK(DrawsConsumed(before, scored.gameplay_rng) == 2);
}

TEST_CASE("the celebration length is set and bounded") {
    const MatchState s = ScoreAGoal();
    CHECK(s.globals.show_fans_counter >= kCelebrationBaseTicks);
    CHECK(s.globals.show_fans_counter <=
          kCelebrationBaseTicks + 63 + 3 * kCelebrationMarginTicks);
}

// --- near miss --------------------------------------------------------------

TEST_CASE("a near miss suppresses the whistle, an ordinary out of play does not") {
    MatchState s{};
    s.ball.speed = kNearMissMinSpeed;
    const int16_t y = static_cast<int16_t>(kPlayableMinY - 1);
    CHECK(IsNearMiss(s, 336, y, 0));            // on target, fast, low
    CHECK_FALSE(IsNearMiss(s, 100, y, 0));      // wide of the frame
    CHECK_FALSE(IsNearMiss(s, 336, y, 40));     // sailed over
    CHECK_FALSE(IsNearMiss(s, 336, kCentreSpotY, 0)); // still on the pitch

    s.ball.speed = static_cast<int16_t>(kNearMissMinSpeed - 1);
    CHECK_FALSE(IsNearMiss(s, 336, y, 0));      // a trickle is not a near miss
}

// --- R5: the six disagreements ----------------------------------------------

// The switches are the deliverable, not a verdict. Each must exist and must
// default to reading A — the behaviour the engine already had — so that a trace
// decides, not whoever edited the header last. A default that has drifted to the
// Amiga's side without a trace to justify it is exactly what this pins against.
TEST_CASE("every disagreement switch defaults to the current reading") {
    CHECK_FALSE(kFoulFromBehindInverted);
    CHECK_FALSE(kAftertouchLatchInverted);
    CHECK_FALSE(kCrossbarSetsSpeed);
    CHECK_FALSE(kFlat3IsDeflection);
    CHECK(kPassLoftEnabled);
}

TEST_CASE("the aftertouch latch helper is the complement it claims to be") {
    // Whatever the switch says, the two readings must be exact complements for
    // every non-trivial pair — that is what makes #2 a one-token A/B.
    for (int joy = 0; joy < 8; ++joy) {
        for (int ref = 0; ref < 8; ++ref) {
            const int a = (joy - ref) & 7;
            const int b = (ref - joy) & 7;
            CAPTURE(joy);
            CAPTURE(ref);
            CHECK(((a + b) & 7) == 0);
            CHECK(AftertouchLatchDiff(joy, ref) == (kAftertouchLatchInverted ? b : a));
        }
    }
}

// --- restart turn masks -----------------------------------------------------

// B13 / R4. Each corner permits exactly the three octants that point infield
// from that flag. The bug this replaced was invisible to every existing test:
// both top corners shared one mask, so a top-left taker could face out of play.
TEST_CASE("each corner mask permits only octants pointing into the pitch") {
    struct Case { uint8_t mask; int a, b, c; };
    const Case cases[] = {
        {kTurnFlagsCornerTopLeft,  2, 3, 4},
        {kTurnFlagsCornerTopRight, 4, 5, 6},
        {kTurnFlagsCornerBotLeft,  0, 1, 2},
        {kTurnFlagsCornerBotRight, 0, 6, 7},
    };
    for (const Case& c : cases) {
        CAPTURE(c.mask);
        int permitted = 0;
        for (int oct = 0; oct < 8; ++oct)
            if (c.mask & (1u << oct)) ++permitted;
        CHECK(permitted == 3);
        CHECK((c.mask & (1u << c.a)) != 0);
        CHECK((c.mask & (1u << c.b)) != 0);
        CHECK((c.mask & (1u << c.c)) != 0);
    }
    // The four are genuinely distinct — the defect was two of them being equal.
    CHECK(kTurnFlagsCornerTopLeft != kTurnFlagsCornerTopRight);
    CHECK(kTurnFlagsCornerBotLeft != kTurnFlagsCornerBotRight);
}

TEST_CASE("a CPU taker is denied the horizontal axis") {
    for (uint8_t m : {kTurnFlagsCornerTopLeft, kTurnFlagsCornerTopRight,
                      kTurnFlagsCornerBotLeft, kTurnFlagsCornerBotRight,
                      kTurnFlagsThrowLeft, kTurnFlagsThrowRight}) {
        const uint8_t cpu = static_cast<uint8_t>(m & kTurnFlagsCpuMask);
        CHECK((cpu & (1u << 2)) == 0); // due east
        CHECK((cpu & (1u << 6)) == 0); // due west
        CHECK((cpu & m) == cpu);       // never permits more than the human mask
    }
}

// The celebration counter is read by the camera as "freeze on the celebration".
// R4 began writing it at every goal without anything ticking it down, so after
// the first goal the camera froze on that goal for the rest of the match. A
// counter that is written but never decremented is a latch, not a timer.
TEST_CASE("the celebration counter counts down to zero") {
    MatchState s = ScoreAGoal();
    const int16_t start = s.globals.show_fans_counter;
    REQUIRE(start > 0);

    int16_t prev = start;
    for (int i = 0; i < start + 8; ++i) {
        UpdateTime(s);
        CHECK(s.globals.show_fans_counter <= prev);
        prev = s.globals.show_fans_counter;
        if (prev == 0) break;
    }
    CHECK(s.globals.show_fans_counter == 0);
    // And it does not go negative once it has expired.
    UpdateTime(s);
    CHECK(s.globals.show_fans_counter == 0);
}
