#pragma once
// Scripted kick / aftertouch fixture — B6a behavioural tests.
//
// Why a fixture rather than hand-built MatchState: the defects B6a fixes only
// exist in the Step order (window arming, fire classification, possession
// during a charge). A test that calls ApplyAftertouchForTeam on a synthetic
// state cannot see any of them. Everything here drives MatchEngine::Step.
//
// Both sides are marked human so no CPU brain runs inside a measurement
// window; the opposing side is fed a neutral stick.

#include "core/aftertouch.hpp"
#include "core/match_engine.hpp"
#include "core/match_input.hpp"
#include "core/movement.hpp"
#include "core/possession.hpp"
#include "core/shooting.hpp"

#include <array>
#include <cstdint>

namespace at::test {

// Unit step per octant, screen axes (y grows downward). Mirrors
// kDefaultDestinations without the ±1000 scale.
inline constexpr std::array<Dest, 8> kOctantStep = {{
    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
}};

// Rotating an octant clockwise on screen (N -> NE -> E ...) is +1.
inline constexpr Dir OctantPlus(Dir d, int delta) {
    return static_cast<Dir>((static_cast<int>(d) + delta + 16) & 7);
}

// Striker slot used by every scenario here.
inline constexpr int kStrikerSlot = 9;

// One striker with the ball at (px, py); everyone else parked out of the way.
// playing_up = 2 means side 0 defends the bottom and attacks the top goal.
inline MatchEngine MakeKickEngine(int px, int py, uint8_t playing_up = 2,
                                  uint32_t seed = 0xB6A00001u) {
    MatchEngine eng;
    eng.Reset(seed);
    eng.Step(MatchInput{}); // bootstrap: BeginMatchIfNeeded + kickoff placement

    MatchState s = eng.State();
    s.globals.team_playing_up = playing_up;
    for (int side = 0; side < 2; ++side) {
        TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
        tc.player_number = static_cast<uint8_t>(side + 1); // human: no CPU brain
        tc.ball_can_be_controlled = 1;
        tc.controlled_slot = static_cast<int8_t>(side * 11);
        for (auto& row : s.sides[static_cast<size_t>(side)].tactics.cells)
            row.fill(0); // every quadrant maps to one corner cell
        for (auto& sq : s.sides[static_cast<size_t>(side)].squad) {
            sq.attrs.speed = 4;
            sq.attrs.passing = 4;
            sq.attrs.shooting = 4;
            sq.attrs.finishing = 4;
            sq.attrs.ball_control = 4;
        }
    }
    s.sides[0].control.controlled_slot = kStrikerSlot;

    PlacePlayersAtKickoff(s);
    s.players[kStrikerSlot].pos.x = Fix::FromInt(px);
    s.players[kStrikerSlot].pos.y = Fix::FromInt(py);
    s.players[kStrikerSlot].dest_x = static_cast<int16_t>(px);
    s.players[kStrikerSlot].dest_y = static_cast<int16_t>(py);

    GiveBallForTest(s, 0, kStrikerSlot);
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.clock.stoppage_event_timer = 0;
    s.phase = MatchPhase::InPlay;
    eng.LoadState(s);
    return eng;
}

struct KickRun {
    int     launch_tick  = -1;   // Step index on which the window opened
    int     latch_spin   = -1;   // spin_timer value at which a curl side latched
    int16_t launch_speed = 0;
    int16_t speed_after_sample = 0; // ball speed one tick past the vertical sample
    Dest    launch_pos{};
    Dest    final_pos{};
    Fix     peak_z{};
    bool    struck       = false;
    bool    was_pass     = false;
    bool    contested    = false; // striker entered a slide/header instead
    bool    lost_ball    = false; // possession dropped before the strike
};

struct KickScript {
    Dir  kick_dir     = Dir::N;
    Dir  after_dir    = Dir::None; // stick after the strike
    int  react_delay  = 1;         // Steps after launch before after_dir is applied
    int  dribble_ticks = 0;        // run with the ball before pressing fire
    int  hold_ticks   = -1;        // -1 = hold until the strike, else release after N
    int  total_ticks  = 60;
};

// Drives one scripted kick and reports what the engine did.
inline KickRun RunKick(MatchEngine& eng, const KickScript& sc) {
    KickRun r;
    MatchInput in{};
    in.p1.dir = sc.kick_dir;

    for (int t = 0; t < sc.dribble_ticks; ++t) {
        in.p1.fire = false;
        eng.Step(in);
    }

    int prev_spin = eng.State().sides[0].control.spin_timer;
    for (int t = 0; t < sc.total_ticks; ++t) {
        const bool hold = (sc.hold_ticks < 0) ? (r.launch_tick < 0) : (t < sc.hold_ticks);
        in.p1.fire = hold;
        in.p1.dir = (r.launch_tick >= 0 && (t - r.launch_tick) >= sc.react_delay)
                        ? sc.after_dir
                        : sc.kick_dir;
        eng.Step(in);

        const MatchState& s = eng.State();
        const TeamControl& tc = s.sides[0].control;
        const Entity& striker = s.players[kStrikerSlot];

        if (r.launch_tick < 0) {
            if (!tc.player_has_ball && !r.struck) r.lost_ball = true;
            if (static_cast<PlayerState>(striker.player_state) != PlayerState::Normal)
                r.contested = true;
        }
        if (r.launch_tick < 0 && prev_spin == kSpinInactive &&
            tc.spin_timer != kSpinInactive) {
            r.launch_tick  = t;
            r.struck       = true;
            r.was_pass     = tc.pass_in_progress != 0;
            r.launch_speed = s.ball.speed;
            r.launch_pos   = Dest{s.ball.pos.x.Whole(), s.ball.pos.y.Whole()};
            r.lost_ball    = false;
        }
        prev_spin = tc.spin_timer;

        if (r.launch_tick >= 0) {
            if (r.latch_spin < 0 && (tc.spin_cw || tc.spin_ccw))
                r.latch_spin = static_cast<int>(tc.spin_timer) - 1;
            if (t == r.launch_tick + kAftertouchVerticalTick + 1)
                r.speed_after_sample = s.ball.speed;
            if (r.peak_z < s.ball.pos.z) r.peak_z = s.ball.pos.z;
        }
        r.final_pos = Dest{s.ball.pos.x.Whole(), s.ball.pos.y.Whole()};
    }
    return r;
}

// Signed lateral offset of the ball from the straight line it was kicked along.
// Positive = clockwise of the kick direction on screen (N -> +x, E -> +y).
inline int LateralBend(const KickRun& r, Dir kick_dir) {
    if (r.launch_tick < 0 || kick_dir == Dir::None) return 0;
    const Dest d = kOctantStep[static_cast<size_t>(kick_dir)];
    const int dx = r.final_pos.x - r.launch_pos.x;
    const int dy = r.final_pos.y - r.launch_pos.y;
    // Clockwise perpendicular of (d.x, d.y) on a y-down screen is (-d.y, d.x).
    return dx * (-d.y) + dy * (d.x);
}

} // namespace at::test
