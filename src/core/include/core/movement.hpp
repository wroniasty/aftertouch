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

inline int SpeedAttrIndex(uint8_t attr) {
    return attr > 7 ? 7 : static_cast<int>(attr);
}

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

inline Dest TacticsDestForCell(uint8_t cell, int side /*0 home/top, 1 away/bot*/) {
    uint8_t qx = static_cast<uint8_t>((cell >> 4) & 0x0Fu);
    uint8_t qy = static_cast<uint8_t>(cell & 0x0Fu);
    if (qx > 14) qx = 14;
    if (qy > 15) qy = 15;
    Dest d{kPlayerXQuadrantCoords[qx], kPlayerYQuadrantCoords[qy]};
    // Approach nudge: top −4, bottom +4.
    d.x = static_cast<int16_t>(d.x + (side == 0 ? -4 : 4));
    return ClampOffBallDest(d);
}

inline Dest OffBallDestination(const MatchState& s, int side, int ordinal_1_based) {
    const int16_t bx = s.ball.pos.x.Whole();
    const int16_t by = s.ball.pos.y.Whole();
    int q = BallQuadrantIndex(bx, by);
    uint8_t cell = 0;

    if (ordinal_1_based == 1) {
        // Keeper: linear map into own box (MOVEMENT §7).
        const int32_t ball_rel_x = static_cast<int32_t>(bx) - 81;
        int16_t gx = static_cast<int16_t>(285 + ball_rel_x * 103 / 510);
        int16_t gy;
        if (side == 0) {
            gy = static_cast<int16_t>(135 + (static_cast<int32_t>(by) - 129) * 26 / 641);
            if (gy < 135) gy = 135;
            if (gy > 161) gy = 161;
        } else {
            gy = static_cast<int16_t>(737 + (static_cast<int32_t>(by) - 129) * 26 / 641);
            if (gy < 737) gy = 737;
            if (gy > 763) gy = 763;
        }
        if (gx < kOffBallMinX) gx = kOffBallMinX;
        if (gx > kOffBallMaxX) gx = kOffBallMaxX;
        return Dest{gx, gy};
    }

    // Outfield: ordinal 2..11 → tactics row 0..9.
    int role = ordinal_1_based - 2;
    if (role < 0) role = 0;
    if (role >= kMatchTacticRoles) role = kMatchTacticRoles - 1;

    if (side == 1) {
        q = 34 - q;
        const auto& row = s.sides[1].tactics.cells[static_cast<size_t>(role)];
        cell = row[static_cast<size_t>(q)];
        cell = static_cast<uint8_t>(0xEF - cell);
    } else {
        const auto& row = s.sides[0].tactics.cells[static_cast<size_t>(role)];
        cell = row[static_cast<size_t>(q)];
    }
    return TacticsDestForCell(cell, side);
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

inline void UpdateControlledPlayer(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    const int base = side * 11;

    // Always refresh ball distances for this side.
    for (int i = 0; i < 11; ++i) {
        Entity& e = s.players[static_cast<size_t>(base + i)];
        e.ball_distance = SquaredBallDistance(e, s.ball);
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
        if (slot == tc.pass_to_slot || slot == tc.passing_kicking_slot) continue;
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

inline void ApplyOffBallDestination(MatchState& s, int side, int slot) {
    Entity& e = s.players[static_cast<size_t>(slot)];
    if (IsSpecialMovementState(e)) return;
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

inline void PlacePlayersAtKickoff(MatchState& s) {
    // Ensure identity slots if Reset-only (no ApplyKickoff).
    for (int side = 0; side < 2; ++side) {
        if (s.sides[static_cast<size_t>(side)].control.team_number == 0)
            s.sides[static_cast<size_t>(side)].control.team_number =
                static_cast<uint8_t>(side + 1);
        if (s.sides[static_cast<size_t>(side)].control.controlled_slot < 0)
            s.sides[static_cast<size_t>(side)].control.controlled_slot =
                static_cast<int8_t>(side * 11);
        // Ball is capturable until a kick lockout (B6) clears this.
        s.sides[static_cast<size_t>(side)].control.ball_can_be_controlled = 1;

        for (int i = 0; i < 11; ++i) {
            const int slot = side * 11 + i;
            Entity& e = s.players[static_cast<size_t>(slot)];
            e.team_number = static_cast<int16_t>(side + 1);
            e.player_ordinal = static_cast<int16_t>(i + 1);
            e.player_state = static_cast<uint8_t>(PlayerState::Normal);
            const Dest d = OffBallDestination(s, side, i + 1);
            e.pos.x = Fix::FromInt(d.x);
            e.pos.y = Fix::FromInt(d.y);
            e.pos.z = Fix{};
            e.dest_x = d.x;
            e.dest_y = d.y;
            e.delta = {};
            e.speed = 0;
            e.direction = (side == 0) ? 4 : 0; // face opponent's half
            e.player_direction = e.direction;
        }
    }
}

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

    UpdatePlayerBeingPassedTo(s, side);
    UpdateControlledPlayer(s, side);
    RefineControlledSelection(s, side);
    PickCpuRestartTaker(s, side);

    // B5: bands + capture before dest/speed so on-ball cut applies same tick.
    UpdatePossessionForSide(s, side);
    TickWonTheBallTimer(tc);

    // B9: CPU brain writes stick/fire; human maps device stick.
    if (tc.player_number == 0)
        AI_SetControlsDirection(s, side);
    else
        MapInputToTeam(tc, pin);

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

    // Dribble after carrier speed. Skip strike/contest tick, and while Fire is
    // held so a charged shot is not dribbled off the player's feet mid-hold.
    if (!struck && !contested && !tc.fire_pressed && tc.fire_counter == 0)
        ApplyDribble(s, side);

    // Cache ball position for the team.
    tc.ball_x = s.ball.pos.x.Whole();
    tc.ball_y = s.ball.pos.y.Whole();
}

} // namespace at
