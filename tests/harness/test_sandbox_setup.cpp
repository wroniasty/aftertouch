// C1b sandbox mode — the config → MatchState projection.
// No SDL: the builder is deliberately pure so it can be asserted on here.
#include <doctest/doctest.h>

#include "core/goalkeeper.hpp"
#include "core/hash.hpp"
#include "core/match_engine.hpp"
#include "core/movement.hpp"
#include "mode/sandbox.hpp"

#include <cstdlib>

using namespace at;
using namespace at::mode;

namespace {

int CountOnPitch(const MatchState& s, int side) {
    int n = 0;
    for (int i = 0; i < 11; ++i)
        if (!IsOffPitch(s.players[static_cast<size_t>(side * 11 + i)])) ++n;
    return n;
}

} // namespace

TEST_CASE("sandbox spawns the requested side and one opposing keeper") {
    SandboxConfig cfg = DefaultSandboxConfig();
    cfg.outfield_count = 4;
    cfg.own_keeper = false;

    MatchState s{};
    BuildSandboxState(cfg, s);

    CHECK(CountOnPitch(s, 0) == 4);
    CHECK(CountOnPitch(s, 1) == 1);
    CHECK_FALSE(IsOffPitch(s.players[11]));      // opposing keeper
    CHECK(IsOffPitch(s.players[0]));             // no keeper of our own
    CHECK(s.sides[1].control.controlled_slot == -1);

    SUBCASE("own keeper is added on request") {
        cfg.own_keeper = true;
        BuildSandboxState(cfg, s);
        CHECK(CountOnPitch(s, 0) == 5);
        CHECK_FALSE(IsOffPitch(s.players[0]));
    }
}

TEST_CASE("spawned players stand on the pitch, absent ones do not") {
    SandboxConfig cfg = DefaultSandboxConfig();
    cfg.outfield_count = 3;

    MatchState s{};
    BuildSandboxState(cfg, s);

    for (int i = 0; i < kPitchPlayers; ++i) {
        const Entity& e = s.players[static_cast<size_t>(i)];
        const int16_t x = e.pos.x.Whole();
        const int16_t y = e.pos.y.Whole();
        if (IsOffPitch(e)) {
            CHECK(x < kPlayableMinX);
            CHECK(e.speed == 0);
        } else {
            CHECK(x >= kPlayableMinX);
            CHECK(x <= kPlayableMaxX);
            CHECK(y >= kPlayableMinY);
            CHECK(y <= kPlayableMaxY);
        }
    }
    // Controlled player is one of the spawned ones.
    const int ctrl = s.sides[0].control.controlled_slot;
    REQUIRE(ctrl >= 0);
    CHECK_FALSE(IsOffPitch(s.players[static_cast<size_t>(ctrl)]));
}

TEST_CASE("attacking spawn takes the forward tactic roles") {
    SandboxConfig cfg = DefaultSandboxConfig();
    cfg.outfield_count = 3;

    cfg.spawn_as_attackers = true;
    CHECK(SandboxFieldSlot(cfg, 0) == 8);  // ordinals 9, 10, 11
    CHECK(SandboxFieldSlot(cfg, 2) == 10);

    cfg.spawn_as_attackers = false;
    CHECK(SandboxFieldSlot(cfg, 0) == 1);  // ordinals 2, 3, 4
    CHECK(SandboxFieldSlot(cfg, 2) == 3);
    CHECK(SandboxFieldSlot(cfg, 3) == -1);
}

TEST_CASE("direction of play is the operator's, not the RNG's") {
    SandboxConfig cfg = DefaultSandboxConfig();

    MatchState s{};
    cfg.attack_down = true;
    BuildSandboxState(cfg, s);
    CHECK(s.globals.team_playing_up == 1);
    CHECK(SideDefendsTop(s, 0));
    CHECK(OppGoalCentre(s, 0).y == kPlayableMaxY);

    cfg.attack_down = false;
    BuildSandboxState(cfg, s);
    CHECK(s.globals.team_playing_up == 2);
    CHECK_FALSE(SideDefendsTop(s, 0));
    CHECK(OppGoalCentre(s, 0).y == kPlayableMinY);

    // A Step must not re-roll it — the match is already started.
    MatchEngine eng;
    StartSandbox(eng, cfg);
    for (int i = 0; i < 10; ++i) eng.Step(MatchInput{});
    CHECK(eng.State().globals.team_playing_up == 2);
}

TEST_CASE("attributes reach the spawned players") {
    SandboxConfig cfg = DefaultSandboxConfig();
    cfg.outfield_count = 2;
    // B13 / R2: 15 / 14 were above the legal range and clamped to 7 on the way
    // into every table, so "fastest" and "second fastest" were the same player.
    cfg.field[0].attrs.speed = kAttrMax;
    cfg.field[0].attrs.finishing = kAttrMax - 1;
    cfg.field[1].attrs.speed = 1;
    cfg.opponent_keeper.goalie_skill = 3;

    MatchState s{};
    BuildSandboxState(cfg, s);

    const int a = SandboxFieldSlot(cfg, 0);
    const int b = SandboxFieldSlot(cfg, 1);
    const SquadPlayer* sa = SquadForPitchSlot(s, a);
    const SquadPlayer* sb = SquadForPitchSlot(s, b);
    REQUIRE(sa);
    REQUIRE(sb);
    CHECK(sa->attrs.speed == kAttrMax);
    CHECK(sa->attrs.finishing == kAttrMax - 1);
    CHECK(sb->attrs.speed == 1);
    CHECK(s.sides[1].squad[0].goalie_skill == 3);

    // Faster player really is faster on the pitch.
    CHECK(LookupPlayerSpeed(s, a, false) > LookupPlayerSpeed(s, b, false));
}

TEST_CASE("the same config produces the same match twice") {
    SandboxConfig cfg = DefaultSandboxConfig();
    cfg.outfield_count = 5;
    cfg.seed = 0xC1B0FEEDu;

    MatchEngine a;
    MatchEngine b;
    StartSandbox(a, cfg);
    StartSandbox(b, cfg);
    CHECK(HashState(a.State()) == HashState(b.State()));

    MatchInput in{};
    in.p1.dir = Dir::S;
    for (int i = 0; i < 120; ++i) {
        in.p1.fire = (i % 17) == 0;
        a.Step(in);
        b.Step(in);
    }
    CHECK(HashState(a.State()) == HashState(b.State()));
    // And it is a live match, not a frozen one.
    CHECK(a.State().tick == 120);
    CHECK(GetPl(a.State()) == GameStatePl::InProgress);
}

TEST_CASE("sandbox survives a hundred seconds of play") {
    SandboxConfig cfg = DefaultSandboxConfig();
    cfg.outfield_count = 3;
    cfg.field[0].attrs.speed = kAttrMax;

    MatchEngine eng;
    StartSandbox(eng, cfg);

    MatchInput in{};
    for (int i = 0; i < 5000; ++i) {
        in.p1.dir = static_cast<Dir>((i / 50) & 7);
        in.p1.fire = (i % 90) == 0;
        eng.Step(in);
    }

    // Whatever the ball did, a restart must resolve rather than deadlock —
    // a keeper-only side has to be able to take one.
    bool live = GetPl(eng.State()) == GameStatePl::InProgress;
    for (int i = 0; i < 600 && !live; ++i) {
        in.p1.dir = Dir::S;
        in.p1.fire = (i % 7) == 0;
        eng.Step(in);
        live = GetPl(eng.State()) == GameStatePl::InProgress;
    }
    CHECK(live);

    const MatchState& s = eng.State();
    // Absent players never joined in.
    for (int i = 0; i < kPitchPlayers; ++i) {
        const Entity& e = s.players[static_cast<size_t>(i)];
        if (IsOffPitch(e)) CHECK(e.pos.x.Whole() < kPlayableMinX);
    }
    // The lone keeper minds his own goal: he heads back goal-side of the ball.
    //
    // This used to read `dest_y >= ball y` with a comment asserting side 1
    // defends the bottom line. It does not always: ends swap at half time, and
    // 5000 ticks crosses one. The check passed by luck under the old default
    // attributes and broke the moment B13 / R2 changed them, which makes it a
    // fixture-dependent test, not an invariant. Stated against the goal line the
    // keeper is actually defending it is direction-agnostic.
    //
    // The surviving `pos.y > kPenaltyBoxTopY` had the same defect and was left
    // in — it only reads as "not in the far box" while side 1 defends the bottom.
    // After the swap his own box *is* the top one. Stated properly: he stays in
    // the penalty area he is defending, which B13 / R7 is what finally makes true
    // (the old rest rule let him wander to the halfway line).
    const Entity& gk = s.players[11];
    const int goal_y = OwnGoalLineY(s, 1);
    const bool defends_top = goal_y < kCentreSpotY;
    if (defends_top)
        CHECK(gk.pos.y.Whole() <= kPenaltyBoxTopY);
    else
        CHECK(gk.pos.y.Whole() >= kPenaltyBoxBotY);

    if (static_cast<PlayerState>(gk.player_state) == PlayerState::Normal) {
        const int to_goal_from_dest = std::abs(gk.dest_y - goal_y);
        const int to_goal_from_ball = std::abs(s.ball.pos.y.Whole() - goal_y);
        CHECK(to_goal_from_dest <= to_goal_from_ball);
    }
}
