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

// [PROVISIONAL: LEGACY §15 "AI: shooting" — human fire-hold threshold in ticks]
// Counted every game tick for humans. B6 §2.1 and the community estimate both
// say 4; 12 is what shipped. Fitting it needs the oracle (B6a Track M), so it
// stays tagged rather than re-guessed.
inline constexpr int16_t kFireHoldThreshold = 12;
inline constexpr int16_t kPassKickLockoutTicks = 25;

// Hold counter saturation: normal_fire is a level, so the counter runs for as
// long as the button is down and must not wrap into a tap.
inline constexpr int16_t kFireCounterMax = 30000;

// [CANDIDATE: amiga ballKickingSpeed asm:30730, ballKickingDeltaZ asm:30729]
// Flat launch for everyone; the attribute bonuses below are what differentiate.
inline constexpr int16_t kBallKickingSpeed = 2208;
inline constexpr int32_t kBallKickingDeltaZRaw = 0x14000; // 1.25 in 16.16
inline constexpr int32_t kBallPassingDeltaZRaw = 0;

// [CANDIDATE: amiga — Finishing bonus −288…+608, Velocity bonus −384…+384]
//
// These were 0…280 and 0…336: bonuses that could only ever *add*. They are
// **signed**, which is a different mechanic, not a rescale — a low-Velocity
// player now loses launch speed rather than merely failing to gain any, and the
// spread across the attribute more than doubles.
//
// PROVENANCE, precisely: the ledger gives the two **endpoints** of each table,
// not their interiors. Finishing's endpoints are 896 apart across 7 steps, an
// exact stride of **128** — a power-of-two stride is characteristic of a real
// table and this one is very likely right as written. Velocity's endpoints are
// 768 apart across 7 steps, which is 109.71 and not clean, so its interior is a
// **linear interpolation of ours** and is tagged as such. Do not cite the
// interior of kBallSpeedKicking as measured; see B13 §6.
inline constexpr std::array<int16_t, 8> kBallSpeedFinishing = {
    -288, -160, -32, 96, 224, 352, 480, 608}; // stride 128, endpoints sourced
inline constexpr std::array<int16_t, 8> kBallSpeedKicking = {
    -384, -274, -165, -55, 55, 165, 274, 384}; // endpoints sourced, interior ours

static_assert(kBallSpeedFinishing.front() == -288 && kBallSpeedFinishing.back() == 608,
              "Finishing bonus endpoints are the sourced part of the table");
static_assert(kBallSpeedKicking.front() == -384 && kBallSpeedKicking.back() == 384,
              "Velocity bonus endpoints are the sourced part of the table");

// [CANDIDATE: amiga loc_110856 asm:34941 — the full descending chain]
//
// Pass strength is banded by **distance to the receiver**, not by how long fire
// was held. The whole table is now sourced, not just its endpoints: the Amiga
// compares the receiver's *squared* ball-distance against these thresholds, so
// the comparison stays integer and no square root is needed anywhere.
inline constexpr std::array<int32_t, 8> kPassBandMaxDistSq = {
    2500, 10000, 22500, 40000, 62500, 90000, 122500, 0x7fffffff};
inline constexpr std::array<int16_t, 8> kBallPassingSpeedByBand = {
    1536, 1664, 1792, 1877, 1962, 2048, 2133, 2218};

static_assert(kBallPassingSpeedByBand.front() == 0x600 &&
                  kBallPassingSpeedByBand.back() == 0x8AA,
              "pass ramp endpoints match the named literals");

// A pass with nobody in the cone is a clearance in the direction you were
// facing, at a flat speed (amiga no_closest_player asm:34994, word_110690).
inline constexpr int16_t kPassClearanceSpeed = 1792; // $700

// [CANDIDATE: amiga unk_110670 asm:34980] Passing bonus on top of the band.
inline constexpr std::array<int16_t, 8> kBallSpeedPassingIncrease = {
    0, 48, 96, 144, 192, 256, 320, 384};

// Squared planar distance from ball to receiver → band index.
inline constexpr int PassDistanceBandFromSq(int32_t dist_sq) {
    for (int band = 0; band < 7; ++band)
        if (dist_sq < kPassBandMaxDistSq[static_cast<size_t>(band)]) return band;
    return 7;
}

// B13 / R2 invariant: one entry per attribute value.
static_assert(kBallSpeedKicking.size() == kAttrTableSize);
static_assert(kBallSpeedFinishing.size() == kAttrTableSize);
static_assert(kBallSpeedPassingIncrease.size() == kAttrTableSize);

// [PROVISIONAL: LEGACY §15 "Timing and integration" — pitch coordinate scale]
// SHOOTING §3's classifier compares the ball's x against 241/431 and its y
// against 204/342/556/694. The x pair is used below as the on-target corridor;
// the y quartet is *not* used as the penalty area, because 342/556 disagree
// with the pitch geometry the rest of the engine runs on (kPenaltyBoxTopY /
// kPenaltyBoxBotY, 216/682) by a factor of two in depth. Which of the two is
// the reference's real area line is a coordinate-scale question and needs the
// oracle; until then the engine's own area is authoritative and the raw
// thresholds stay recorded here as the fit target.
inline constexpr int16_t kShotZoneLeft  = 241;
inline constexpr int16_t kShotZoneRight = 431;
inline constexpr int16_t kRefShotBoxTopY   = 204;
inline constexpr int16_t kRefShotBoxMidTop = 342;
inline constexpr int16_t kRefShotBoxMidBot = 556;
inline constexpr int16_t kRefShotBoxBotY   = 694;

// Which speed table a strike earns, if any (SHOOTING §3).
enum class ShotZone : uint8_t {
    None      = 0, // not a shot on goal: no bonus at all
    LongShot  = 1, // on target from outside the area: Velocity
    Finishing = 2, // on target from inside the area: Finishing
};

// Set (never clear) the tap/hold flags. Clearing happens when the owning side
// runs ApplyKickOrPass / drops the flag — otherwise a flag raised on the other
// team's control tick would be lost before the kicker acts.
//
// normal_fire is a **level, not an edge**: the reference's on-ball dispatch
// re-reads it every frame (SHOOTING §1), so a hold that is not strikeable on
// the exact tick the counter crosses the threshold — no direction, ball out of
// the close band, mid-contest — must still fire as soon as it becomes
// strikeable. Raising it once and dropping it swallowed the shot and forced a
// release plus a fresh full-length hold (B6a / S3).
inline void ClassifyFireFlags(TeamControl& tc, bool fire_down, int16_t prior_counter,
                              bool prior_pressed) {
    if (fire_down) {
        const int32_t next = static_cast<int32_t>(prior_counter) + 1;
        if (next >= kFireHoldThreshold)
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
            pin.fire ? static_cast<int16_t>(prior_counter < kFireCounterMax
                                                ? prior_counter + 1
                                                : kFireCounterMax)
                     : int16_t{0};
    }
}

inline bool KickDirectionOk(const TeamControl& tc) {
    return tc.current_allowed_direction >= 0 && tc.current_allowed_direction <= 7;
}

inline bool NearBallForKick(const TeamControl& tc) {
    return tc.pl_very_close_to_ball || tc.pl_close_to_ball;
}

// team_playing_up defends the top goal, so it attacks increasing Y ("down").
inline bool SideAttacksDown(const MatchState& s, int side) {
    return s.globals.team_playing_up == static_cast<uint8_t>(side + 1);
}

// Goalward octants for the goal this side is attacking.
inline bool KickIsGoalward(const MatchState& s, int side, int dir) {
    if (SideAttacksDown(s, side))
        return dir == 3 || dir == 4 || dir == 5; // SE, S, SW
    return dir == 0 || dir == 1 || dir == 7;     // N, NE, NW
}

// The penalty area this side is attacking, in the engine's own pitch geometry.
inline bool InAttackingPenaltyArea(const MatchState& s, int side, int16_t x,
                                   int16_t y) {
    if (x < kShotZoneLeft || x > kShotZoneRight) return false;
    return SideAttacksDown(s, side) ? (y >= kPenaltyBoxBotY) : (y <= kPenaltyBoxTopY);
}

// SHOOTING §3: direction *and* position decide whether a kick is a shot on
// goal, and off-target kicks earn nothing. The old test was octant-only, so a
// hoof upfield from inside your own area was paid as a shot — and its "box"
// was a union of two definitions covering a third of the pitch (B6a / S5).
inline ShotZone ClassifyShotOnGoal(const MatchState& s, int side, int16_t x,
                                   int16_t y, int dir) {
    if (dir < 0 || dir > 7) return ShotZone::None;
    if (!KickIsGoalward(s, side, dir)) return ShotZone::None;
    // Wide of the goal corridor: not a shot on goal.
    if (x < kShotZoneLeft || x > kShotZoneRight) return ShotZone::None;
    // Behind the halfway line: not a shot on goal either.
    if (SideAttacksDown(s, side) ? (y < kCentreSpotY) : (y > kCentreSpotY))
        return ShotZone::None;
    return InAttackingPenaltyArea(s, side, x, y) ? ShotZone::Finishing
                                                 : ShotZone::LongShot;
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

// --- pass targeting (amiga GetClosestNonControlledPlayerInDirection asm:39859)
//
// Rewritten from the Amiga reading. Three things here were wrong and together
// they are why passes went "in the facing direction most of the time":
//
//  1. **There was a maximum range.** There is none in the original. The old cap
//     was ~70–126 units, so on a 510x641 pitch almost no teammate qualified and
//     nearly every pass degraded to the no-target clearance — which is exactly
//     the reported symptom.
//  2. **The cone was anchored at the passer.** It is anchored at the **ball**:
//     the candidate's angle is measured from the ball, not from the kicker. The
//     two differ throughout a dribble, which is when passes are actually made.
//  3. **The cone was +-1 octant (+-45 degrees), quantised.** It is +-16/256 —
//     **+-22.5 degrees** — tested on the raw 256-step angle, so it is half as
//     wide and not snapped to octants.
//
// Filters (asm:39865-39880): not the controlled player, not sent off, must be
// PL_NORMAL. The keeper is *not* excluded — a pass back to him is legal.
// Among survivors the smallest ball-distance wins.
inline constexpr int kPassConeHalfWidth = 16; // 16/256 of a turn = 22.5 degrees

inline bool PassCandidateInCone(const MatchState& s, int slot, int facing_octant) {
    const Entity& t = s.players[static_cast<size_t>(slot)];
    const Dest ball{s.ball.pos.x.Whole(), s.ball.pos.y.Whole()};
    const Dest at{t.pos.x.Whole(), t.pos.y.Whole()};
    if (ball.x == at.x && ball.y == at.y) return false;
    const int16_t to_target = AngleTo(ball, at);
    if (to_target < 0) return false;
    // Signed byte difference against the facing octant expressed in 256ths.
    const int centre = (facing_octant << 5) & 0xFF;
    int diff = (static_cast<int>(to_target) - centre) & 0xFF;
    if (diff > 127) diff -= 256;
    return diff >= -kPassConeHalfWidth && diff <= kPassConeHalfWidth;
}

inline bool PassCandidateEligible(const MatchState& s, int slot, int controlled) {
    if (slot == controlled) return false;
    const Entity& t = s.players[static_cast<size_t>(slot)];
    if (t.cards < 0 || t.sent_away) return false;
    if (static_cast<PlayerState>(t.player_state) != PlayerState::Normal) return false;
    if (IsOffPitch(t)) return false;
    return true;
}

// Nearest-to-the-ball teammate inside the cone, or -1. No range limit.
inline int FindPassTargetSlot(const MatchState& s, int side, int kicker_slot,
                              int facing_octant) {
    if (facing_octant < 0 || facing_octant > 7) return -1;
    const int base = side * 11;
    const Dest ball{s.ball.pos.x.Whole(), s.ball.pos.y.Whole()};
    int best = -1;
    int32_t best_d = 0x7fffffff;
    for (int i = 0; i < 11; ++i) {
        const int slot = base + i;
        if (!PassCandidateEligible(s, slot, kicker_slot)) continue;
        if (!PassCandidateInCone(s, slot, facing_octant)) continue;
        const Entity& t = s.players[static_cast<size_t>(slot)];
        const int32_t dx = static_cast<int32_t>(t.pos.x.Whole()) - ball.x;
        const int32_t dy = static_cast<int32_t>(t.pos.y.Whole()) - ball.y;
        const int32_t d = dx * dx + dy * dy;
        if (d < best_d) {
            best_d = d;
            best = slot;
        }
    }
    return best;
}

// [CANDIDATE: amiga calculate_pass_to_player_delta_x_y asm:34913]
//
// The aim point is the ball->receiver vector **doubled until it leaves the
// pitch's bounding box**, so the ball travels *through* the receiver rather than
// stopping at him. Direction is preserved exactly (doubling both components is a
// pure scale).
//
// This is not cosmetic. Aiming *at* the receiver makes the heading unstable as
// the ball closes (the engine re-derives delta from dest every tick), and it
// made aftertouch catastrophic: the curl nudges dest by up to 16 per tick and
// ~368 over a full window, which on a 40-unit pass vector swamps the direction
// entirely and can reverse it. That is the reported "aftertouch turns the ball
// 180 degrees". With the ray extended the same nudge is a few degrees.
inline constexpr int16_t kAimBoxW = 672;
inline constexpr int16_t kAimBoxH = 880;

inline Dest ExtendPassAim(Dest ball, Dest target) {
    int32_t dx = static_cast<int32_t>(target.x) - ball.x;
    int32_t dy = static_cast<int32_t>(target.y) - ball.y;
    if (dx == 0 && dy == 0) dx = 1; // degenerate guard (asm:34917)
    // Double *while the current aim point is still inside*, so the loop exits
    // with it outside — that is what makes the heading stable for the whole
    // flight. Bounded so a degenerate input cannot spin.
    for (int guard = 0; guard < 16; ++guard) {
        const int32_t ax = ball.x + dx;
        const int32_t ay = ball.y + dy;
        if (ax < 0 || ax >= kAimBoxW || ay < 0 || ay >= kAimBoxH) break;
        dx *= 2;
        dy *= 2;
    }
    return Dest{static_cast<int16_t>(ball.x + dx), static_cast<int16_t>(ball.y + dy)};
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

        // Re-pick every time. The cone is anchored at the ball and this tick's
        // facing, so a candidate cached from an earlier tick can easily no
        // longer be in it.
        const int target = FindPassTargetSlot(s, side, slot, dir);

        if (target >= 0) {
            const Entity& t = s.players[static_cast<size_t>(target)];
            const Dest aim = ExtendPassAim(
                Dest{bx, by}, Dest{t.pos.x.Whole(), t.pos.y.Whole()});
            ball.dest_x = aim.x;
            ball.dest_y = aim.y;
            tc.pass_to_slot = static_cast<int8_t>(target);
            // Band on the distance to the **receiver**, not to the aim point —
            // the aim point is deliberately off the pitch and means nothing here.
            const int32_t pdx = static_cast<int32_t>(t.pos.x.Whole()) - bx;
            const int32_t pdy = static_cast<int32_t>(t.pos.y.Whole()) - by;
            const int band = PassDistanceBandFromSq(pdx * pdx + pdy * pdy);
            speed = static_cast<int16_t>(
                kBallPassingSpeedByBand[static_cast<size_t>(band)] +
                kBallSpeedPassingIncrease[static_cast<size_t>(idx)]);
        } else {
            // Nobody in the cone: a clearance in the facing direction at a flat
            // speed. No band, no Passing bonus (amiga asm:34994).
            const Dest off = kDefaultDestinations[static_cast<size_t>(dir)];
            ball.dest_x = static_cast<int16_t>(bx + off.x);
            ball.dest_y = static_cast<int16_t>(by + off.y);
            tc.pass_to_slot = -1;
            speed = kPassClearanceSpeed;
        }
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

        const ShotZone zone = ClassifyShotOnGoal(s, side, bx, by, dir);
        if (zone != ShotZone::None) {
            const bool inbox = (zone == ShotZone::Finishing);
            const int16_t attr = SquadAttr(s, kicker, inbox);
            const int idx = AttrIndex0to7(static_cast<uint8_t>(attr));
            speed = static_cast<int16_t>(
                speed + (inbox ? kBallSpeedFinishing[static_cast<size_t>(idx)]
                               : kBallSpeedKicking[static_cast<size_t>(idx)]));
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
    tc.spin_timer = kSpinArmed;
    tc.spin_cw = 0;
    tc.spin_ccw = 0;
    tc.long_pass = 0;
    tc.long_spin_pass = 0;

    // Consume fire pulses so we do not double-strike.
    tc.quick_fire = 0;
    tc.normal_fire = 0;
    return true;
}

} // namespace at
