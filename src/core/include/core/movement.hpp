#pragma once
#include "core/angle.hpp"
#include "core/match_clock.hpp"
#include "core/match_input.hpp"
#include "core/match_state.hpp"
#include "core/ai.hpp"
#include "core/goalkeeper.hpp"
#include "core/possession.hpp"
#include "core/set_pieces.hpp"
#include "core/shooting.hpp"
#include "core/tackling.hpp"
#include "core/trig.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

// Player movement — doc/implementation/B4-player-movement.md / doc/MOVEMENT.md.
// Decision (ApplyTeamControls) writes dest/speed/delta; MovePlayers integrates.

namespace at {

// --- constants (MOVEMENT.md §10) -------------------------------------------

inline constexpr std::array<int16_t, 8> kPlayerSpeedsGameInProgress = {
    928, 974, 1020, 1066, 1112, 1158, 1204, 1250};
inline constexpr std::array<int16_t, 8> kPlayerSpeedsGameStopped = {
    1136, 1152, 1168, 1184, 1200, 1216, 1232, 1248};
inline constexpr std::array<int16_t, 8> kInjuriesSpeedHandicap = {
    0, -96, -128, -160, -192, -224, -256, -288};

// B13 / R2 invariant: an attribute-indexed table has one entry per attribute
// value. kInjuriesSpeedHandicap is indexed by injury_level/32, not an attribute,
// but is the same width by construction.
static_assert(kPlayerSpeedsGameInProgress.size() == kAttrTableSize);
static_assert(kPlayerSpeedsGameStopped.size() == kAttrTableSize);
static_assert(kInjuriesSpeedHandicap.size() == kAttrTableSize);

inline constexpr int16_t kPlayerGroundConstant = 96;
inline constexpr int16_t kPlayerAirConstant    = 72;
inline constexpr int16_t kMaxAnimSpeed         = 1280;

// Controlled-player stop test (slightly outside playable).
inline constexpr int16_t kCtrlBoundMinX = 79;
inline constexpr int16_t kCtrlBoundMaxX = 592;
inline constexpr int16_t kCtrlBoundMinY = 127;
inline constexpr int16_t kCtrlBoundMaxY = 771;

// Forbidden-direction bitmasks (bit i = octant i).
inline constexpr uint8_t kMaskLeft  = 0xE0; // 7,6,5
inline constexpr uint8_t kMaskRight = 0x0E; // 1,2,3
inline constexpr uint8_t kMaskUp    = 0x83; // 0,1,7
inline constexpr uint8_t kMaskDown  = 0x38; // 3,4,5

// Ball quadrant limits — 5 columns × 7 rows = 35.
inline constexpr std::array<int16_t, 5> kBallXQuadrantLimits = {
    81, 183, 285, 387, 489};
inline constexpr std::array<int16_t, 7> kBallYQuadrantLimits = {
    129, 220, 312, 403, 495, 586, 678};

// Player tactics grid → pitch coords (15 × 16).
inline constexpr std::array<int16_t, 15> kPlayerXQuadrantCoords = {
    98, 132, 166, 200, 234, 268, 302, 336, 370, 404, 438, 472, 506, 540, 574};
inline constexpr std::array<int16_t, 16> kPlayerYQuadrantCoords = {
    149, 189, 229, 269, 309, 349, 389, 429, 469, 509, 549, 589, 629, 669, 709,
    749};

inline constexpr int16_t kOffBallMinX = 81;
inline constexpr int16_t kOffBallMaxX = 590;
inline constexpr int16_t kOffBallMinY = 129;
inline constexpr int16_t kOffBallMaxY = 769;

// --- helpers ---------------------------------------------------------------

// Named for the call site; the range lives in match_state.hpp (B13 / R2).
inline int SpeedAttrIndex(uint8_t attr) { return AttrIndex0to7(attr); }

inline void StopEntity(Entity& e) {
    e.dest_x = e.pos.x.Whole();
    e.dest_y = e.pos.y.Whole();
    e.delta.x = Fix{};
    e.delta.y = Fix{};
}

inline int PitchSlotForSide(int side, int ordinal_1_based) {
    return side * 11 + (ordinal_1_based - 1);
}

inline SquadPlayer* SquadForPitchSlot(MatchState& s, int slot) {
    if (slot < 0 || slot >= kPitchPlayers) return nullptr;
    const Entity& e = s.players[static_cast<size_t>(slot)];
    if (e.team_number != 1 && e.team_number != 2) return nullptr;
    const int side = e.team_number - 1;
    const int ord  = e.player_ordinal;
    if (ord < 1 || ord > kMatchSquadSize) return nullptr;
    return &s.sides[static_cast<size_t>(side)].squad[static_cast<size_t>(ord - 1)];
}

inline const SquadPlayer* SquadForPitchSlot(const MatchState& s, int slot) {
    return SquadForPitchSlot(const_cast<MatchState&>(s), slot);
}

inline int32_t SquaredBallDistance(const Entity& e, const Entity& ball) {
    const int32_t dx = static_cast<int32_t>(e.pos.x.Whole()) -
                       static_cast<int32_t>(ball.pos.x.Whole());
    const int32_t dy = static_cast<int32_t>(e.pos.y.Whole()) -
                       static_cast<int32_t>(ball.pos.y.Whole());
    return dx * dx + dy * dy;
}

inline int BallQuadrantIndex(int16_t bx, int16_t by) {
    int col = 0;
    for (int i = 0; i < 5; ++i) {
        if (bx >= kBallXQuadrantLimits[static_cast<size_t>(i)]) col = i;
    }
    int row = 0;
    for (int i = 0; i < 7; ++i) {
        if (by >= kBallYQuadrantLimits[static_cast<size_t>(i)]) row = i;
    }
    int idx = row * 5 + col;
    if (idx < 0) idx = 0;
    if (idx > 34) idx = 34;
    return idx;
}

inline Dest ClampOffBallDest(Dest d) {
    if (d.x < kOffBallMinX) d.x = kOffBallMinX;
    if (d.x > kOffBallMaxX) d.x = kOffBallMaxX;
    if (d.y < kOffBallMinY) d.y = kOffBallMinY;
    if (d.y > kOffBallMaxY) d.y = kOffBallMaxY;
    return d;
}

inline Dest TacticsDestForCell(uint8_t cell, bool defend_top) {
    uint8_t qx = static_cast<uint8_t>((cell >> 4) & 0x0Fu);
    uint8_t qy = static_cast<uint8_t>(cell & 0x0Fu);
    if (qx > 14) qx = 14;
    if (qy > 15) qy = 15;
    Dest d{kPlayerXQuadrantCoords[qx], kPlayerYQuadrantCoords[qy]};
    // Approach nudge: defending-top −4, defending-bottom +4.
    d.x = static_cast<int16_t>(d.x + (defend_top ? -4 : 4));
    return ClampOffBallDest(d);
}

// Legacy overload: 0 = defend top, 1 = defend bottom.
inline Dest TacticsDestForCell(uint8_t cell, int side) {
    return TacticsDestForCell(cell, side == 0);
}

// Which end this side defends. Prefer team_playing_up; if unset, home=top.
inline bool SideDefendsTop(const MatchState& s, int side) {
    const uint8_t up = s.globals.team_playing_up;
    if (up == 1 || up == 2)
        return up == static_cast<uint8_t>(side + 1);
    return side == 0;
}

inline Dest OffBallDestination(const MatchState& s, int side, int ordinal_1_based) {
    const int16_t bx = s.ball.pos.x.Whole();
    const int16_t by = s.ball.pos.y.Whole();
    int q = BallQuadrantIndex(bx, by);
    uint8_t cell = 0;
    // team_playing_up defends the top goal / attacks down (SIMULATION.md).
    const bool defend_top = SideDefendsTop(s, side);

    if (ordinal_1_based == 1) {
        // Keeper: the one Amiga map, shared with ApplyGoalkeeperAI's rest branch.
        // This used to be a second copy of the formula (with a span of 26 rather
        // than 27), and the copy that actually ran — the one in the keeper AI —
        // was a different rule entirely. One function, both callers.
        return KeeperRestDestination(s, side);
    }

    // Outfield: ordinal 2..11 → tactics row 0..9.
    int role = ordinal_1_based - 2;
    if (role < 0) role = 0;
    if (role >= kMatchTacticRoles) role = kMatchTacticRoles - 1;

    const auto& row =
        s.sides[static_cast<size_t>(side)].tactics.cells[static_cast<size_t>(role)];
    if (!defend_top) {
        q = 34 - q;
        cell = row[static_cast<size_t>(q)];
        cell = static_cast<uint8_t>(0xEF - cell);
    } else {
        cell = row[static_cast<size_t>(q)];
    }
    return TacticsDestForCell(cell, defend_top);
}

inline uint8_t BoundaryMask(int16_t x, int16_t y) {
    uint8_t m = 0;
    if (x < kCtrlBoundMinX) m = static_cast<uint8_t>(m | kMaskLeft);
    if (x > kCtrlBoundMaxX) m = static_cast<uint8_t>(m | kMaskRight);
    if (y < kCtrlBoundMinY) m = static_cast<uint8_t>(m | kMaskUp);
    if (y > kCtrlBoundMaxY) m = static_cast<uint8_t>(m | kMaskDown);
    return m;
}

inline int8_t ApplyTurnFlags(int8_t dir, uint8_t flags) {
    if (flags == 0 || flags == 0xFF) return dir;
    if (dir >= 0 && dir <= 7 && (flags & (1u << static_cast<uint8_t>(dir))))
        return dir;
    // Walk down from 7 to find first allowed.
    for (int d = 7; d >= 0; --d) {
        if (flags & (1u << static_cast<uint8_t>(d)))
            return static_cast<int8_t>(d);
    }
    return -1;
}

inline int16_t LookupPlayerSpeed(const MatchState& s, int slot, bool controlled) {
    const SquadPlayer* sp = SquadForPitchSlot(s, slot);
    const int idx = SpeedAttrIndex(sp ? sp->attrs.speed : 0);
    const bool in_progress = GetPl(s) == GameStatePl::InProgress;
    int32_t speed = in_progress ? kPlayerSpeedsGameInProgress[static_cast<size_t>(idx)]
                                : kPlayerSpeedsGameStopped[static_cast<size_t>(idx)];

    const Entity& e = s.players[static_cast<size_t>(slot)];
    const int inj = static_cast<int>(e.injury_level) / 32;
    const int ih = (inj < 0) ? 0 : (inj > 7 ? 7 : inj);
    speed += kInjuriesSpeedHandicap[static_cast<size_t>(ih)];

    const int side = e.team_number - 1;
    if (controlled && side >= 0 && side < 2 &&
        s.sides[static_cast<size_t>(side)].control.player_has_ball)
        speed -= speed / 8;

    if (speed < 0) speed = 0;
    if (speed > 32767) speed = 32767;
    return static_cast<int16_t>(speed);
}

inline void SetFrameDelayFromSpeed(Entity& e) {
    const int v = std::max(static_cast<int>(kMaxAnimSpeed) - static_cast<int>(e.speed), 0);
    e.frame_delay = static_cast<int16_t>(v / 128 + 6);
}

inline void WriteDeltasFromDest(Entity& e) {
    const Dest from{e.pos.x.Whole(), e.pos.y.Whole()};
    const Dest dest{e.dest_x, e.dest_y};
    const DeltaResult d = CalculateDeltaXAndY(e.speed, from, dest);
    e.delta.x = d.dx;
    e.delta.y = d.dy;
    if (d.heading >= 0) {
        e.full_direction = d.heading;
        e.direction = static_cast<int16_t>(
            ((static_cast<uint16_t>(d.heading) + 16u) & 0xFFu) >> 5);
        e.is_moving = 1;
    } else {
        e.delta.x = Fix{};
        e.delta.y = Fix{};
        e.is_moving = 0;
    }
}

inline void ApplyNonNormalDecay(Entity& e) {
    const auto st = static_cast<PlayerState>(e.player_state);
    if (st == PlayerState::Tackling || st == PlayerState::Tackled) {
        int32_t sp = e.speed - kPlayerGroundConstant;
        if (sp < 0) sp = 0;
        e.speed = static_cast<int16_t>(sp);
        if (e.speed == 0) StopEntity(e);
    } else if (st == PlayerState::JumpHeader) {
        int32_t sp = e.speed - kPlayerAirConstant;
        if (sp < 0) sp = 0;
        e.speed = static_cast<int16_t>(sp);
        if (e.speed == 0) StopEntity(e);
    }
}

inline bool IsSpecialMovementState(const Entity& e) {
    const auto st = static_cast<PlayerState>(e.player_state);
    return st == PlayerState::Tackling || st == PlayerState::Tackled ||
           st == PlayerState::JumpHeader || st == PlayerState::StaticHeader ||
           st == PlayerState::Injured;
}

inline bool IsEligibleForControl(const Entity& e, const TeamControl& tc) {
    if (e.cards < 0) return false; // sent off
    if (e.player_ordinal == 1 && !tc.goalie_playing_or_out) return false;
    if (IsSpecialMovementState(e)) return false;
    return true;
}

// Anyone this side could put on the ball. Ignores the B9 pass/kicker
// exclusions: a side whose only candidate is the pass target still has a team.
inline bool HasEligibleFieldPlayer(const MatchState& s, int side) {
    const TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    const int base = side * 11;
    for (int i = 0; i < 11; ++i) {
        if (IsEligibleForControl(s.players[static_cast<size_t>(base + i)], tc))
            return true;
    }
    return false;
}

inline void UpdateControlledPlayer(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    const int base = side * 11;

    // Always refresh ball distances for this side.
    for (int i = 0; i < 11; ++i) {
        Entity& e = s.players[static_cast<size_t>(base + i)];
        e.ball_distance = SquaredBallDistance(e, s.ball);
    }

    // Nobody to control — a side reduced to its keeper, or a C1b sandbox side
    // built without one. Release the slot in open play so slot 0 stays on
    // ApplyGoalkeeperAI instead of being steered by the outfield brain (which
    // would dribble the keeper upfield). A restart taker keeps the slot until
    // play resumes; the possession pass drops player_has_ball on its own.
    if (GetPl(s) == GameStatePl::InProgress && !HasEligibleFieldPlayer(s, side)) {
        if (tc.controlled_slot >= 0 && tc.controlled_slot < kPitchPlayers) {
            StopEntity(s.players[static_cast<size_t>(tc.controlled_slot)]);
            tc.controlled_slot = -1;
        }
        return;
    }

    if (!tc.ball_out_of_play) return; // hold slot while "in play" possession
    if (tc.player_switch_timer > 0) return; // B9 lockout after pass arrival

    int best = -1;
    int32_t best_d = 0x7fffffff;
    for (int i = 0; i < 11; ++i) {
        const int slot = base + i;
        Entity& e = s.players[static_cast<size_t>(slot)];
        if (!IsEligibleForControl(e, tc)) continue;
        // B9: exclude pass target + striker so the two selections never collide.
        // As in RefineControlledSelection: only a committed receiver is excluded.
        if ((tc.pass_in_progress && slot == tc.pass_to_slot) ||
            slot == tc.passing_kicking_slot)
            continue;
        if (e.ball_distance < best_d) {
            best_d = e.ball_distance;
            best = slot;
        }
    }
    if (best < 0 || best == tc.controlled_slot) return;

    if (tc.controlled_slot >= 0 && tc.controlled_slot < kPitchPlayers)
        StopEntity(s.players[static_cast<size_t>(tc.controlled_slot)]);
    tc.controlled_slot = static_cast<int8_t>(best);
}

inline void ApplyControlledDestination(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    if (tc.controlled_slot < 0 || tc.controlled_slot >= kPitchPlayers) return;
    Entity& e = s.players[static_cast<size_t>(tc.controlled_slot)];
    // Slide/header lock facing + dest at entry (B7).
    if (IsSpecialMovementState(e)) return;

    int8_t dir = static_cast<int8_t>(tc.current_allowed_direction);

    // Restart takes: stick aims only — no translation until fire (B8 / C1a play).
    // Neutral stick keeps the last facing (throw-in default into pitch).
    // StartingGame only while Stopped (open play keeps StartingGame + InProgress).
    if (IsActiveRestartTake(s)) {
        if (dir >= 0 && dir <= 7) {
            dir = ApplyTurnFlags(dir, s.globals.player_turn_flags);
            if (dir >= 0 && dir <= 7) {
                e.direction = dir;
                e.player_direction = dir;
                tc.direction = dir;
                tc.controlled_pl_direction = dir;
                tc.current_allowed_direction = dir;
            }
        }
        StopEntity(e);
        e.speed = 0;
        return;
    }

    if (GetPl(s) != GameStatePl::InProgress) {
        dir = ApplyTurnFlags(dir, s.globals.player_turn_flags);
        tc.current_allowed_direction = dir;
    }

    if (dir < 0 || dir > 7) {
        StopEntity(e);
        return;
    }

    const uint8_t mask = BoundaryMask(e.pos.x.Whole(), e.pos.y.Whole());
    if (mask & (1u << static_cast<uint8_t>(dir))) {
        StopEntity(e);
        return;
    }

    const Dest off = kDefaultDestinations[static_cast<size_t>(dir)];
    e.dest_x = static_cast<int16_t>(e.pos.x.Whole() + off.x);
    e.dest_y = static_cast<int16_t>(e.pos.y.Whole() + off.y);
}

// amiga PASSING §5 — the receiver of an in-flight pass gets one of two
// destinations: his own position when the ball is coming to him, or a point 90
// degrees off the ball's flight line when it is not, so he steps into its path.
// We only ever did the first, which is part of why an off-target pass sailed
// past a receiver who never moved.
inline constexpr int32_t kReceiverOnPathTolSq = 144; // 12 units either side
inline constexpr int16_t kReceiverStepIn      = 24;

inline void ApplyReceiverDestination(MatchState& s, Entity& e) {
    const Entity& ball = s.ball;
    const int16_t px = e.pos.x.Whole();
    const int16_t py = e.pos.y.Whole();
    const int16_t bx = ball.pos.x.Whole();
    const int16_t by = ball.pos.y.Whole();

    // Perpendicular distance from the receiver to the ball's flight line, using
    // the ball's own delta as the line direction.
    const int32_t vx = ball.delta.x.Whole();
    const int32_t vy = ball.delta.y.Whole();
    if (vx == 0 && vy == 0) { // not travelling: nothing to step into
        StopEntity(e);
        e.speed = 0;
        return;
    }
    const int32_t rx = static_cast<int32_t>(px) - bx;
    const int32_t ry = static_cast<int32_t>(py) - by;
    const int32_t cross = vx * ry - vy * rx;
    const int32_t len2 = vx * vx + vy * vy;
    // Perpendicular distance is |cross| / sqrt(len2). Squaring both sides turns
    // the test into cross^2 <= tol^2 * len2 — integer throughout, no divide and
    // no sqrt, so it stays inside the no-float wall.
    const bool on_path = static_cast<int64_t>(cross) * cross <=
                         static_cast<int64_t>(kReceiverOnPathTolSq) * len2;

    if (on_path) {
        StopEntity(e);
        e.speed = 0;
        return;
    }
    // Step 90 degrees off the flight line, toward the line.
    const int32_t sign = (cross > 0) ? -1 : 1;
    const int32_t nx = sign * -vy;
    const int32_t ny = sign * vx;
    const int32_t nlen = (nx < 0 ? -nx : nx) + (ny < 0 ? -ny : ny);
    if (nlen == 0) {
        StopEntity(e);
        e.speed = 0;
        return;
    }
    e.dest_x = static_cast<int16_t>(px + nx * kReceiverStepIn / nlen);
    e.dest_y = static_cast<int16_t>(py + ny * kReceiverStepIn / nlen);
    e.is_moving = 1;
}

inline void ApplyOffBallDestination(MatchState& s, int side, int slot) {
    Entity& e = s.players[static_cast<size_t>(slot)];
    if (IsSpecialMovementState(e)) return;
    {
        const TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
        // Restart shortfall: keep walking toward the approach stand.
        if (tc.restart_shortfall < 0 && slot == tc.pass_to_slot &&
            IsShortfallAssistState(GetGameState(s)))
            return;
        // Dead-ball: squad freezes (taker aims via ApplyControlledDestination).
        if (GetPl(s) != GameStatePl::InProgress) {
            StopEntity(e);
            e.speed = 0;
            return;
        }
        // AI.md §2.2 / amiga PASSING §5: the designated receiver of an **in-flight**
        // pass either stands still (the ball is coming to him) or steps into the
        // ball's path (it is not). A mere aiming candidate does neither — freezing
        // him on every tick of open play was why team-mates stopped dead for no
        // visible reason, and why the wrong man was selected for a loose ball.
        if (tc.pass_in_progress && slot == tc.pass_to_slot) {
            ApplyReceiverDestination(s, e);
            return;
        }
    }
    const Dest d = OffBallDestination(s, side, e.player_ordinal);
    e.dest_x = d.x;
    e.dest_y = d.y;
}

inline void ApplySpeedAndDeltasForSlot(MatchState& s, int slot, bool controlled) {
    Entity& e = s.players[static_cast<size_t>(slot)];
    const auto st = static_cast<PlayerState>(e.player_state);

    if (st == PlayerState::Tackling || st == PlayerState::JumpHeader ||
        st == PlayerState::Tackled) {
        ApplyNonNormalDecay(e);
        // Keep heading dest if still moving.
        if (e.speed > 0 && e.direction >= 0 && e.direction <= 7) {
            const Dest off = kDefaultDestinations[static_cast<size_t>(e.direction)];
            e.dest_x = static_cast<int16_t>(e.pos.x.Whole() + off.x);
            e.dest_y = static_cast<int16_t>(e.pos.y.Whole() + off.y);
        }
        WriteDeltasFromDest(e);
        SetFrameDelayFromSpeed(e);
        return;
    }

    if (st != PlayerState::Normal && st != PlayerState::Unknown) {
        // Other non-normal: freeze in place this tick.
        StopEntity(e);
        return;
    }

    // Parked (dest == pos): stay still — do not reassign stopped-table speed.
    if (e.dest_x == e.pos.x.Whole() && e.dest_y == e.pos.y.Whole()) {
        e.speed = 0;
        e.delta.x = Fix{};
        e.delta.y = Fix{};
        e.is_moving = 0;
        SetFrameDelayFromSpeed(e);
        if (!controlled) {
            const Dest from{e.pos.x.Whole(), e.pos.y.Whole()};
            const Dest to{s.ball.pos.x.Whole(), s.ball.pos.y.Whole()};
            const int16_t h = AngleTo(from, to);
            if (h >= 0) {
                e.full_direction = h;
                e.direction = static_cast<int16_t>(
                    ((static_cast<uint16_t>(h) + 128u + 16u) & 0xFFu) >> 5);
            }
        }
        return;
    }

    e.speed = LookupPlayerSpeed(s, slot, controlled);
    WriteDeltasFromDest(e);

    // Idle controlled player keeps facing; idle off-ball faces ball.
    if (!e.is_moving) {
        if (!controlled) {
            const Dest from{e.pos.x.Whole(), e.pos.y.Whole()};
            const Dest to{s.ball.pos.x.Whole(), s.ball.pos.y.Whole()};
            const int16_t h = AngleTo(from, to);
            if (h >= 0) {
                e.full_direction = h;
                e.direction = static_cast<int16_t>(
                    ((static_cast<uint16_t>(h) + 128u + 16u) & 0xFFu) >> 5);
            }
        }
    }
    SetFrameDelayFromSpeed(e);
}

inline void MapInputToTeam(TeamControl& tc, const PlayerInput& in) {
    // Stick only — fire is refreshed every tick in RefreshHumanFire.
    // CPU stick/fire written by AI_SetControlsDirection (B9).
    if (tc.player_number == 0) return;
    const int8_t dir = static_cast<int8_t>(in.dir);
    tc.direction = dir;
    tc.current_allowed_direction = dir;
}

// --- public API ------------------------------------------------------------
// PlacePlayersAtKickoff declared in set_pieces.hpp; defined in match_engine.cpp.

inline void MoveSprite(Entity& e) {
    if (e.delta.x.Raw() != 0) {
        const bool pos_dir = e.delta.x.Raw() > 0;
        e.pos.x += e.delta.x;
        const int16_t x = e.pos.x.Whole();
        if (pos_dir ? (e.dest_x <= x) : (e.dest_x >= x)) {
            e.pos.x = Fix::FromInt(e.dest_x);
            e.delta.x = Fix{};
        }
    }
    if (e.delta.y.Raw() != 0) {
        const bool pos_dir = e.delta.y.Raw() > 0;
        e.pos.y += e.delta.y;
        const int16_t y = e.pos.y.Whole();
        if (pos_dir ? (e.dest_y <= y) : (e.dest_y >= y)) {
            e.pos.y = Fix::FromInt(e.dest_y);
            e.delta.y = Fix{};
        }
    }
}

inline void MovePlayers(MatchState& s) {
    for (int i = 0; i < kPitchPlayers; ++i)
        MoveSprite(s.players[static_cast<size_t>(i)]);
}

inline void ApplyTeamControls(MatchState& s, const MatchInput& in) {
    ++s.globals.team_switch_counter;
    const int side = s.globals.team_switch_counter & 1;

    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    const PlayerInput& pin = (side == 0) ? in.p1 : in.p2;

    UpdateControlledPlayer(s, side);
    RefineControlledSelection(s, side);
    // Human + CPU: pick taker and park at spot while dead-ball.
    PickCpuRestartTaker(s, side);

    // B5: bands + capture before dest/speed so on-ball cut applies same tick.
    UpdatePossessionForSide(s, side);
    TickWonTheBallTimer(tc);

    // B9: CPU brain writes stick/fire; human maps device stick.
    if (tc.player_number == 0)
        AI_SetControlsDirection(s, side);
    else
        MapInputToTeam(tc, pin);

    // Pass candidate after stick so facing cone matches this tick's aim.
    UpdatePlayerBeingPassedTo(s, side);
    // Wall-clock shortfall: tick the taking side every frame, not only on its turn.
    if (GetPl(s) != GameStatePl::InProgress &&
        IsShortfallAssistState(GetGameState(s))) {
        const uint8_t take = s.globals.last_team_played_before_break;
        if (take >= 1 && take <= 2)
            TickRestartShortfall(s, static_cast<int>(take) - 1);
    }

    // B8 restart take while Stopped; else B7/B6 open-play fork.
    bool contested = false;
    bool struck = false;
    if (GetPl(s) != GameStatePl::InProgress) {
        struck = ApplyRestartTake(s, side);
    } else if (tc.player_has_ball) {
        struck = ApplyKickOrPass(s, side);
    } else {
        contested = TryBeginSlideOrHeader(s, side);
    }
    // Drop unused fire pulses on this side's turn (sticky across off-ticks).
    if (!struck && !contested) {
        tc.quick_fire = 0;
        tc.normal_fire = 0;
    }

    const int base = side * 11;
    const int controlled = tc.controlled_slot;

    for (int i = 0; i < 11; ++i) {
        const int slot = base + i;
        const bool is_ctrl = (slot == controlled);
        // Off the pitch: no tactics recall, no keeper AI. The existing dest is
        // still integrated so a sending-off walk finishes, then he parks.
        if (IsOffPitch(s.players[static_cast<size_t>(slot)])) {
            ApplySpeedAndDeltasForSlot(s, slot, false);
            continue;
        }
        if (is_ctrl) {
            ApplyControlledDestination(s, side);
        } else if (i == 0) {
            // Uncontrolled keeper: rest / claim / dive (B9).
            ApplyGoalkeeperAI(s, side);
        } else {
            ApplyOffBallDestination(s, side, slot);
        }
        ApplySpeedAndDeltasForSlot(s, slot, is_ctrl);
    }

    // Dribble after carrier speed, skipping only the tick that struck or
    // entered a contest. Holding Fire used to suppress it so a charged shot was
    // not kicked off the player's own foot — but the ball then stayed put while
    // the carrier ran on, left the close band inside the hold, and the queued
    // strike arrived as a slide (B6a / S4). Possession during a charge is
    // ordinary possession; the strike tick short-circuits the dribble anyway.
    if (!struck && !contested) ApplyDribble(s, side);

    AddCarryDistanceForCarrier(s, side);

    // Cache ball position for the team.
    tc.ball_x = s.ball.pos.x.Whole();
    tc.ball_y = s.ball.pos.y.Whole();
}

} // namespace at
