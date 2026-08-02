#pragma once
#include "core/angle.hpp"
#include "core/ball.hpp"
#include "core/match_clock.hpp"
#include "core/match_input.hpp"
#include "core/match_state.hpp"

#include <array>
#include <cstdint>

// Kick / pass launch — doc/implementation/B6-shooting.md / doc/SHOOTING.md.
// Table values are provisional fit targets (LEGACY §15).

namespace at {

// Counted every game tick for humans. ~0.24 s @ 50 Hz — short jab = pass.
inline constexpr int16_t kFireHoldThreshold = 12;
inline constexpr int16_t kPassKickLockoutTicks = 25;

// Provisional launch constants (playable placeholders until A3 traces).
// Passes stay on the ground; shots get a modest loft (lob via aftertouch).
inline constexpr int16_t kBallKickingSpeed = 2800;
inline constexpr int16_t kBallPassingSpeed = 2000;
inline constexpr int32_t kBallKickingDeltaZRaw = 70000; // ~1 whole-unit/tick
inline constexpr int32_t kBallPassingDeltaZRaw = 0;

inline constexpr std::array<int16_t, 8> kBallSpeedKicking = {
    0, 40, 80, 120, 160, 200, 240, 280}; // by Velocity
inline constexpr std::array<int16_t, 8> kBallSpeedFinishing = {
    0, 48, 96, 144, 192, 240, 288, 336}; // by Finishing
inline constexpr std::array<int16_t, 8> kBallSpeedPassingIncrease = {
    80, 100, 120, 140, 160, 180, 200, 220};

// SHOOTING goalward / box thresholds (provisional).
inline constexpr int16_t kShotZoneLeft  = 241;
inline constexpr int16_t kShotZoneRight = 431;
inline constexpr int16_t kShotBoxTopY   = 204;
inline constexpr int16_t kShotBoxMidTop = 342;
inline constexpr int16_t kShotBoxMidBot = 556;
inline constexpr int16_t kShotBoxBotY   = 694;

inline int AttrIndex0to7(uint8_t attr) {
    return attr > 7 ? 7 : static_cast<int>(attr);
}

// Set (never clear) tap/hold pulses. Clearing happens when the owning side
// runs ApplyKickOrPass / drops the pulse — otherwise a pulse on the other
// team's control tick would be lost before the kicker acts.
inline void ClassifyFireFlags(TeamControl& tc, bool fire_down, int16_t prior_counter,
                              bool prior_pressed) {
    if (fire_down) {
        const int16_t next = static_cast<int16_t>(prior_counter + 1);
        if (next == kFireHoldThreshold)
            tc.normal_fire = 1;
    } else if (prior_pressed && prior_counter >= 1 &&
               prior_counter < kFireHoldThreshold) {
        tc.quick_fire = 1;
    }
}

// Advance human fire every tick (wall-clock hold threshold).
inline void RefreshHumanFire(MatchState& s, const MatchInput& in) {
    for (int side = 0; side < 2; ++side) {
        TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
        if (tc.player_number == 0) {
            // B9: CPU brain sets fire on its control tick — do not wipe here.
            continue;
        }
        const PlayerInput& pin = (side == 0) ? in.p1 : in.p2;
        const bool prior_pressed = tc.fire_pressed != 0;
        const int16_t prior_counter = tc.fire_counter;
        ClassifyFireFlags(tc, pin.fire, prior_counter, prior_pressed);
        tc.fire_this_frame =
            static_cast<uint8_t>((pin.fire && !prior_pressed) ? 1 : 0);
        tc.fire_pressed = static_cast<uint8_t>(pin.fire ? 1 : 0);
        tc.fire_counter =
            pin.fire ? static_cast<int16_t>(prior_counter + 1) : int16_t{0};
    }
}

inline bool KickDirectionOk(const TeamControl& tc) {
    return tc.current_allowed_direction >= 0 && tc.current_allowed_direction <= 7;
}

inline bool NearBallForKick(const TeamControl& tc) {
    return tc.pl_very_close_to_ball || tc.pl_close_to_ball;
}

inline bool InPenaltyBox(int16_t x, int16_t y) {
    if (x < kShotZoneLeft || x > kShotZoneRight) return false;
    return (y <= kShotBoxMidTop && y >= kShotBoxTopY) ||
           (y >= kShotBoxMidBot && y <= kShotBoxBotY) ||
           (y <= kPenaltyBoxTopY) || (y >= kPenaltyBoxBotY);
}

// Goalward: team_playing_up attacks increasing Y (bottom / "down").
inline bool KickIsGoalward(const MatchState& s, int side, int dir) {
    const uint8_t up = s.globals.team_playing_up;
    const bool attack_down = (up == static_cast<uint8_t>(side + 1));
    if (attack_down)
        return dir == 3 || dir == 4 || dir == 5; // SE, S, SW
    return dir == 0 || dir == 1 || dir == 7;     // N, NE, NW
}

inline int16_t SquadAttr(const MatchState& s, const Entity& e, bool finishing) {
    const int side_i = e.team_number - 1;
    if (side_i < 0 || side_i >= 2) return 0;
    const int ord = e.player_ordinal;
    if (ord < 1 || ord > kMatchSquadSize) return 0;
    const auto& a = s.sides[static_cast<size_t>(side_i)]
                        .squad[static_cast<size_t>(ord - 1)]
                        .attrs;
    return finishing ? a.finishing : a.shooting;
}

inline int16_t SquadPassingAttr(const MatchState& s, const Entity& e) {
    const int side_i = e.team_number - 1;
    if (side_i < 0 || side_i >= 2) return 0;
    const int ord = e.player_ordinal;
    if (ord < 1 || ord > kMatchSquadSize) return 0;
    return s.sides[static_cast<size_t>(side_i)]
        .squad[static_cast<size_t>(ord - 1)]
        .attrs.passing;
}

// Provisional max pass-target range: ~70u + ~8u per Passing point (0–7).
inline int32_t PassTargetMaxDistSq(int passing_attr) {
    const int r = 70 + 8 * AttrIndex0to7(static_cast<uint8_t>(passing_attr));
    return static_cast<int32_t>(r) * r;
}

// True if target lies in ±1 octant of facing and within max_dist_sq.
inline bool PassTargetInConeAndRange(const MatchState& s, int kicker_slot,
                                     int target_slot, int dir,
                                     int32_t max_dist_sq) {
    if (target_slot < 0 || target_slot >= kPitchPlayers) return false;
    if (target_slot == kicker_slot) return false;
    if (dir < 0 || dir > 7) return false;
    const Entity& k = s.players[static_cast<size_t>(kicker_slot)];
    const Entity& t = s.players[static_cast<size_t>(target_slot)];
    const int32_t dx = static_cast<int32_t>(t.pos.x.Whole()) - k.pos.x.Whole();
    const int32_t dy = static_cast<int32_t>(t.pos.y.Whole()) - k.pos.y.Whole();
    if (dx == 0 && dy == 0) return false;
    const int32_t dist = dx * dx + dy * dy;
    if (dist > max_dist_sq) return false;
    const Dest ahead = kDefaultDestinations[static_cast<size_t>(dir)];
    if (dx * ahead.x + dy * ahead.y <= 0) return false;

    int nearest = dir;
    int32_t best_dot = 0x80000000;
    for (int d = 0; d < 8; ++d) {
        const Dest o = kDefaultDestinations[static_cast<size_t>(d)];
        const int32_t dot = dx * o.x + dy * o.y;
        if (dot > best_dot) {
            best_dot = dot;
            nearest = d;
        }
    }
    const int rot = (nearest - dir) & 7;
    const int wrap = rot > 4 ? 8 - rot : rot;
    return wrap <= 1;
}

// Nearest teammate in a ±1-octant facing cone within max_dist_sq.
inline int FindPassTargetSlot(const MatchState& s, int side, int kicker_slot, int dir,
                              int32_t max_dist_sq) {
    const int base = side * 11;
    const int16_t kx = s.players[static_cast<size_t>(kicker_slot)].pos.x.Whole();
    const int16_t ky = s.players[static_cast<size_t>(kicker_slot)].pos.y.Whole();
    const Dest ahead = kDefaultDestinations[static_cast<size_t>(dir)];
    int best = -1;
    int32_t best_d = 0x7fffffff;
    for (int i = 0; i < 11; ++i) {
        const int slot = base + i;
        if (slot == kicker_slot) continue;
        const Entity& t = s.players[static_cast<size_t>(slot)];
        const int32_t dx = static_cast<int32_t>(t.pos.x.Whole()) - kx;
        const int32_t dy = static_cast<int32_t>(t.pos.y.Whole()) - ky;
        if (dx == 0 && dy == 0) continue;
        if (dx * ahead.x + dy * ahead.y <= 0) continue;

        int nearest = dir;
        int32_t best_dot = 0x80000000;
        for (int d = 0; d < 8; ++d) {
            const Dest o = kDefaultDestinations[static_cast<size_t>(d)];
            const int32_t dot = dx * o.x + dy * o.y;
            if (dot > best_dot) {
                best_dot = dot;
                nearest = d;
            }
        }
        const int rot = (nearest - dir) & 7;
        const int wrap = rot > 4 ? 8 - rot : rot;
        if (wrap > 1) continue;

        const int32_t dist = dx * dx + dy * dy;
        if (dist > max_dist_sq) continue;
        if (dist < best_d) {
            best_d = dist;
            best = slot;
        }
    }
    return best;
}

// Returns true if a strike was launched this call.
inline bool ApplyKickOrPass(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    const bool is_pass = tc.quick_fire != 0;
    const bool is_shot = tc.normal_fire != 0;
    if (!is_pass && !is_shot) return false;
    if (GetPl(s) != GameStatePl::InProgress) return false;
    if (!tc.player_has_ball) return false; // B7: fire without ball → contests
    if (!KickDirectionOk(tc) || !NearBallForKick(tc)) return false;
    if (tc.controlled_slot < 0 || tc.controlled_slot >= kPitchPlayers) return false;

    const int slot = tc.controlled_slot;
    Entity& kicker = s.players[static_cast<size_t>(slot)];
    Entity& ball = s.ball;

    int dir = tc.current_allowed_direction;
    if (dir < 0 || dir > 7) dir = kicker.direction;
    if (dir < 0 || dir > 7) dir = 0;

    const int16_t bx = ball.pos.x.Whole();
    const int16_t by = ball.pos.y.Whole();

    int16_t speed = 0;
    Fix delta_z{};

    if (is_pass) {
        const int16_t passing = SquadPassingAttr(s, kicker);
        const int idx = AttrIndex0to7(static_cast<uint8_t>(passing));
        const int32_t max_d = PassTargetMaxDistSq(passing);

        int target = -1;
        if (PassTargetInConeAndRange(s, slot, tc.pass_to_slot, dir, max_d))
            target = tc.pass_to_slot;
        else
            target = FindPassTargetSlot(s, side, slot, dir, max_d);

        if (target >= 0) {
            ball.dest_x = s.players[static_cast<size_t>(target)].pos.x.Whole();
            ball.dest_y = s.players[static_cast<size_t>(target)].pos.y.Whole();
            tc.pass_to_slot = static_cast<int8_t>(target);
        } else {
            // No in-range cone target: ground kick in facing direction.
            const Dest off = kDefaultDestinations[static_cast<size_t>(dir)];
            ball.dest_x = static_cast<int16_t>(bx + off.x);
            ball.dest_y = static_cast<int16_t>(by + off.y);
            tc.pass_to_slot = -1;
        }
        speed = static_cast<int16_t>(kBallPassingSpeed +
                                    kBallSpeedPassingIncrease[static_cast<size_t>(idx)]);
        delta_z = Fix::FromRaw(kBallPassingDeltaZRaw); // ground pass
        tc.pass_in_progress = 1;
        {
            const int sq = SquadIndexFromPitchSlot(slot);
            if (PlayerMatchStats* st = MatchStatsFor(s, side, sq))
                BumpU16(st->passes_attempted);
        }
    } else {
        const Dest off = kDefaultDestinations[static_cast<size_t>(dir)];
        ball.dest_x = static_cast<int16_t>(bx + off.x);
        ball.dest_y = static_cast<int16_t>(by + off.y);
        tc.pass_to_slot = -1;
        tc.pass_in_progress = 0;
        speed = kBallKickingSpeed;
        delta_z = Fix::FromRaw(kBallKickingDeltaZRaw);

        if (KickIsGoalward(s, side, dir)) {
            const bool inbox = InPenaltyBox(bx, by);
            const int16_t attr = SquadAttr(s, kicker, inbox);
            const int idx = AttrIndex0to7(static_cast<uint8_t>(attr));
            if (inbox)
                speed = static_cast<int16_t>(speed +
                                             kBallSpeedFinishing[static_cast<size_t>(idx)]);
            else
                speed = static_cast<int16_t>(
                    speed + kBallSpeedKicking[static_cast<size_t>(idx)]);
        }
    }

    ball.speed = speed;
    ball.delta.z = delta_z;
    ball.pos.z = Fix{}; // leave the foot from the ground
    ball.direction = static_cast<int16_t>(dir);

    // Release possession + lockout.
    tc.player_has_ball = 0;
    tc.pass_kick_timer = kPassKickLockoutTicks;
    tc.ball_can_be_controlled = 0;
    tc.ball_in_play = 1;
    MarkBallLoose(s); // both sides may re-select (AI.md §2.1)
    tc.passing_kicking_slot = static_cast<int8_t>(slot);
    tc.controlled_pl_direction = static_cast<int16_t>(dir);
    s.clock.last_team_played = static_cast<uint8_t>(side + 1);

    ResetBothSpinTimers(s);
    tc.spin_timer = 0;
    tc.left_spin = 0;
    tc.right_spin = 0;
    tc.long_pass = 0;
    tc.long_spin_pass = 0;

    // Consume fire pulses so we do not double-strike.
    tc.quick_fire = 0;
    tc.normal_fire = 0;
    return true;
}

} // namespace at
