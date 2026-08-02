#pragma once
#include "core/angle.hpp"
#include "core/match_clock.hpp"
#include "core/match_input.hpp"
#include "core/match_state.hpp"
#include "core/referee.hpp"

#include <array>
#include <cstdint>

// Set pieces / foul classify / cards — doc/implementation/B8-set-pieces.md.
// Sources: SETPIECES.md, SIMULATION.md §6.

namespace at {

inline constexpr uint8_t kTurnFlagsAll      = 0xFF;
inline constexpr uint8_t kTurnFlagsPenUpper = 0x83; // NW N NE
inline constexpr uint8_t kTurnFlagsPenLower = 0x38; // SW S SE
inline constexpr uint8_t kTurnFlagsKickOffTop    = 0x7C; // E..W southish
inline constexpr uint8_t kTurnFlagsKickOffBottom = 0xC7; // E..W northish
inline constexpr uint8_t kBreakCameraModeRestart = 255;  // -1

inline constexpr int16_t kPenaltySpotUpperY = 187;
inline constexpr int16_t kPenaltySpotLowerY = 711;
inline constexpr int16_t kPenBoxXMin = 193;
inline constexpr int16_t kPenBoxXMax = 478;
inline constexpr int16_t kFkBandUpperMinY = 216;
inline constexpr int16_t kFkBandUpperMaxY = 331; // exclusive
inline constexpr int16_t kFkBandLowerMinY = 567; // exclusive lower bound in doc
inline constexpr int16_t kFkBandLowerMaxY = 682;

inline constexpr int16_t kMaxInjuriesPerSide = 2;
// Match shooting.hpp launch placeholders (avoid circular include).
inline constexpr int16_t kRestartKickSpeed     = 2800;
inline constexpr int16_t kRestartKickPassSpeed = 2000;
inline constexpr int16_t kRestartThrowSpeed    = 2200;
inline constexpr int16_t kRestartThrowPassSpeed = 1400;
inline constexpr int32_t kRestartKickDeltaZRaw = 70000;
inline constexpr int32_t kRestartKickPassDeltaZRaw = 0;
inline constexpr int32_t kRestartThrowDeltaZRaw = 70000; // long throw loft
inline constexpr int32_t kRestartThrowPassDeltaZRaw = 18000;
inline constexpr int16_t kRestartLockoutTicks  = 25;
// ~2 s @ 50 Hz before a teammate approaches for a tap-pass (throw-in / FK).
inline constexpr int16_t kRestartShortfallTicks = 100;
inline constexpr int16_t kRestartApproachUnits = 48;

inline int8_t ClampDirToTurnFlags(int8_t dir, uint8_t flags) {
    if (flags == 0 || flags == 0xFF) return dir;
    if (dir >= 0 && dir <= 7 && (flags & (1u << static_cast<uint8_t>(dir))))
        return dir;
    for (int d = 7; d >= 0; --d) {
        if (flags & (1u << static_cast<uint8_t>(d)))
            return static_cast<int8_t>(d);
    }
    return -1;
}

// Aim tables: 8 Dest per table (SETPIECES §7).
inline constexpr std::array<Dest, 8> kLeftThrowInBallDestDelta = {{
    {250, -1000}, {1000, -1000}, {1000, 0}, {1000, 1000},
    {250, 1000}, {-1000, 1000}, {-1000, 0}, {-1000, -1000},
}};
inline constexpr std::array<Dest, 8> kRightThrowInBallDestDelta = {{
    {-250, -1000}, {1000, -1000}, {1000, 0}, {1000, 1000},
    {-250, 1000}, {-1000, 1000}, {-1000, 0}, {-1000, -1000},
}};
inline constexpr std::array<Dest, 8> kPenaltyBallDestDelta = {{
    {0, -1000}, {500, -1000}, {1000, 0}, {500, 1000},
    {0, 1000}, {-500, 1000}, {-1000, 0}, {-500, -1000},
}};
inline constexpr std::array<Dest, 8> kUpperLeftCornerBallDestDelta = {{
    {0, -1000}, {1000, -1000}, {1000, 150}, {1000, 300},
    {250, 1000}, {-1000, 1000}, {-1000, 0}, {-1000, -1000},
}};
inline constexpr std::array<Dest, 8> kUpperRightCornerBallDestDelta = {{
    {0, -1000}, {1000, -1000}, {1000, 0}, {1000, 1000},
    {-250, 1000}, {-1000, 1000}, {-1000, 150}, {-1000, -1000},
}};
inline constexpr std::array<Dest, 8> kLowerLeftCornerBallDestDelta = {{
    {250, -1000}, {1000, -350}, {1000, -150}, {1000, 1000},
    {0, 1000}, {-1000, 1000}, {-1000, 0}, {-1000, -1000},
}};
// Symmetric mirror of lower-left (IDA db misparse avoided).
inline constexpr std::array<Dest, 8> kLowerRightCornerBallDestDelta = {{
    {-250, -1000}, {-1000, -350}, {-1000, -150}, {-1000, 1000},
    {0, 1000}, {1000, 1000}, {1000, 0}, {1000, -1000},
}};

inline bool IsThrowInState(GameState gs) {
    const auto v = static_cast<uint8_t>(gs);
    return v >= static_cast<uint8_t>(GameState::ThrowInForwardRight) &&
           v <= static_cast<uint8_t>(GameState::ThrowInBackLeft);
}

inline bool IsFreeKickState(GameState gs) {
    const auto v = static_cast<uint8_t>(gs);
    return v >= static_cast<uint8_t>(GameState::FreeKickLeft1) &&
           v <= static_cast<uint8_t>(GameState::FreeKickRight3);
}

inline bool IsCornerState(GameState gs) {
    return gs == GameState::CornerLeft || gs == GameState::CornerRight;
}

inline bool IsGoalKickState(GameState gs) {
    return gs == GameState::GoalOutLeft || gs == GameState::GoalOutRight;
}

inline bool IsRestartTakeState(GameState gs) {
    return IsThrowInState(gs) || IsCornerState(gs) || IsFreeKickState(gs) ||
           gs == GameState::Penalty || gs == GameState::Foul ||
           IsGoalKickState(gs) || gs == GameState::KeeperHoldsBall;
}

// Throw-in / free kick: idle shortfall summons a facing-direction pass target.
inline bool IsShortfallAssistState(GameState gs) {
    return IsThrowInState(gs) || IsFreeKickState(gs);
}

inline void ClearSpinTimers(MatchState& s) {
    s.sides[0].control.spin_timer = -1;
    s.sides[1].control.spin_timer = -1;
}

inline void ParkBallAtSpot(MatchState& s, int16_t x, int16_t y) {
    Entity& ball = s.ball;
    ball.pos.x = Fix::FromInt(x);
    ball.pos.y = Fix::FromInt(y);
    ball.pos.z = Fix{};
    ball.delta = {};
    ball.speed = 0;
    ball.dest_x = x;
    ball.dest_y = y;
}

inline void StopAllPlayers(MatchState& s) {
    for (int i = 0; i < kPitchPlayers; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        e.dest_x = e.pos.x.Whole();
        e.dest_y = e.pos.y.Whole();
        e.delta.x = Fix{};
        e.delta.y = Fix{};
        e.speed = 0;
    }
}

// Select taker for the side that owns the restart (human or CPU).
inline void PickRestartTaker(MatchState& s, int side) {
    if (GetPl(s) == GameStatePl::InProgress) return;
    if (!IsRestartTakeState(GetGameState(s))) return;
    if (s.globals.last_team_played_before_break !=
        static_cast<uint8_t>(side + 1))
        return;

    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    const int base = side * 11;
    int best = base + 1;
    if (GetGameState(s) == GameState::KeeperHoldsBall) {
        best = base; // keeper distributes
    } else if (GetGameState(s) == GameState::Penalty) {
        int best_fin = -1;
        for (int i = 1; i < 11; ++i) {
            const int fin =
                s.sides[static_cast<size_t>(side)].squad[static_cast<size_t>(i)]
                    .attrs.finishing;
            if (fin > best_fin &&
                s.players[static_cast<size_t>(base + i)].cards >= 0) {
                best_fin = fin;
                best = base + i;
            }
        }
    } else {
        int32_t best_d = 0x7fffffff;
        for (int i = 1; i < 11; ++i) {
            const int slot = base + i;
            Entity& e = s.players[static_cast<size_t>(slot)];
            if (e.cards < 0) continue;
            const int32_t dx = e.pos.x.Whole() - s.globals.foul_x;
            const int32_t dy = e.pos.y.Whole() - s.globals.foul_y;
            const int32_t d = dx * dx + dy * dy;
            if (d < best_d) {
                best_d = d;
                best = slot;
            }
        }
    }
    tc.controlled_slot = static_cast<int8_t>(best);
}

inline void BeginRestart(MatchState& s, GameState gs, int16_t spot_x, int16_t spot_y,
                         uint8_t turn_flags, uint8_t camera_dir,
                         uint8_t taking_team) {
    SetGameState(s, gs);
    SetPl(s, GameStatePl::Stopped);
    s.globals.foul_x = spot_x;
    s.globals.foul_y = spot_y;
    s.globals.player_turn_flags = turn_flags;
    s.globals.camera_direction = camera_dir;
    s.globals.last_team_played_before_break = taking_team;
    s.globals.break_camera_mode = kBreakCameraModeRestart;
    ParkBallAtSpot(s, spot_x, spot_y);
    ClearSpinTimers(s);
    StopAllPlayers(s);
    for (int i = 0; i < 2; ++i) {
        TeamControl& tc = s.sides[static_cast<size_t>(i)].control;
        tc.ball_in_play = 0;
        tc.long_pass = 0; // restart shortfall countdown (dead-ball reuse)
    }
    if (IsShortfallAssistState(gs) && taking_team >= 1 && taking_team <= 2) {
        s.sides[static_cast<size_t>(taking_team - 1)].control.long_pass =
            kRestartShortfallTicks;
    }
    MarkBallLoose(s);
}

inline GameState FreeKickZoneForX(int16_t x, bool top_team_fouled) {
    // Zones for top team fouled; mirror for bottom.
    GameState z;
    if (x < 153) z = GameState::FreeKickLeft1;
    else if (x < 261) z = GameState::FreeKickLeft2;
    else if (x < 309) z = GameState::FreeKickLeft3;
    else if (x < 362) z = GameState::FreeKickCentre;
    else if (x < 410) z = GameState::FreeKickRight1;
    else if (x < 518) z = GameState::FreeKickRight2;
    else z = GameState::FreeKickRight3;

    if (!top_team_fouled) {
        // Mirror LEFT↔RIGHT.
        switch (z) {
        case GameState::FreeKickLeft1:  return GameState::FreeKickRight3;
        case GameState::FreeKickLeft2:  return GameState::FreeKickRight2;
        case GameState::FreeKickLeft3:  return GameState::FreeKickRight1;
        case GameState::FreeKickRight1: return GameState::FreeKickLeft3;
        case GameState::FreeKickRight2: return GameState::FreeKickLeft2;
        case GameState::FreeKickRight3: return GameState::FreeKickLeft1;
        default: return z;
        }
    }
    return z;
}

// offending_side: 0 or 1 (tackler's side). Victim position classifies.
inline void ClassifyAndBeginFoulRestart(MatchState& s, int16_t vx, int16_t vy,
                                        int offending_side) {
    if (GetPl(s) != GameStatePl::InProgress) return;
    const GameState cur = GetGameState(s);
    if (IsRestartTakeState(cur)) return;

    const uint8_t taking_team =
        static_cast<uint8_t>((1 - offending_side) + 1); // fouled team takes

    const bool in_pen_x = vx >= kPenBoxXMin && vx <= kPenBoxXMax;
    if (in_pen_x && vy <= kPenaltyBoxTopY) {
        BeginRestart(s, GameState::Penalty, kCentreSpotX, kPenaltySpotUpperY,
                     kTurnFlagsPenUpper, 4, taking_team);
        return;
    }
    if (in_pen_x && vy >= kPenaltyBoxBotY) {
        BeginRestart(s, GameState::Penalty, kCentreSpotX, kPenaltySpotLowerY,
                     kTurnFlagsPenLower, 0, taking_team);
        return;
    }

    const bool upper_fk = vy >= kFkBandUpperMinY && vy < kFkBandUpperMaxY;
    const bool lower_fk = vy > kFkBandLowerMinY && vy <= kFkBandLowerMaxY;
    if (upper_fk || lower_fk) {
        // top_team_fouled: team playing toward top was fouled → taking is that team.
        const bool top_takes =
            (taking_team == s.globals.team_playing_up) ||
            (s.globals.team_playing_up == 0 && taking_team == 1);
        const GameState fk = FreeKickZoneForX(vx, top_takes);
        BeginRestart(s, fk, vx, vy, kTurnFlagsAll, upper_fk ? 4 : 0, taking_team);
        return;
    }

    BeginRestart(s, GameState::Foul, vx, vy, kTurnFlagsAll, 0, taking_team);
}

inline uint8_t TurnFlagsForOop(GameState gs, int16_t spot_x, int16_t spot_y) {
    if (IsThrowInState(gs)) {
        // Into-pitch arc: left touch N..S via E; right N..S via W.
        return (spot_x < kCentreSpotX) ? uint8_t{0x1F} : uint8_t{0xF1};
    }
    if (IsCornerState(gs)) {
        return (spot_y < kCentreSpotY) ? kTurnFlagsPenLower : kTurnFlagsPenUpper;
    }
    if (gs == GameState::Penalty) {
        return (spot_y < kCentreSpotY) ? kTurnFlagsPenUpper : kTurnFlagsPenLower;
    }
    return kTurnFlagsAll;
}

inline uint8_t CameraForOop(GameState gs, int16_t spot_y) {
    (void)gs;
    return (spot_y < kCentreSpotY) ? uint8_t{4} : uint8_t{0};
}

// Taking team for OOP: opposite of last_team_played (who put it out), 1/2.
inline uint8_t TakingTeamForOop(const MatchState& s, GameState /*gs*/, bool is_goal) {
    if (is_goal) return 0; // kick-off handled elsewhere
    const uint8_t last = s.clock.last_team_played;
    if (last == 1) return 2;
    if (last == 2) return 1;
    return 1;
}

inline int CountInjuredOnSide(const MatchState& s, int side) {
    int n = 0;
    const int base = side * 11;
    for (int i = 0; i < 11; ++i) {
        if (s.players[static_cast<size_t>(base + i)].injury_level > 0) ++n;
    }
    return n;
}

inline void PlaceThrowInTaker(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    if (tc.controlled_slot < 0 || tc.controlled_slot >= kPitchPlayers) {
        // Pick nearest outfield on this side to ball.
        int best = side * 11 + 1;
        int32_t best_d = 0x7fffffff;
        for (int i = 1; i < 11; ++i) {
            const int slot = side * 11 + i;
            Entity& e = s.players[static_cast<size_t>(slot)];
            if (e.cards < 0) continue;
            const int32_t dx = e.pos.x.Whole() - s.ball.pos.x.Whole();
            const int32_t dy = e.pos.y.Whole() - s.ball.pos.y.Whole();
            const int32_t d = dx * dx + dy * dy;
            if (d < best_d) {
                best_d = d;
                best = slot;
            }
        }
        tc.controlled_slot = static_cast<int8_t>(best);
    }
    Entity& e = s.players[static_cast<size_t>(tc.controlled_slot)];
    const int16_t bx = s.ball.pos.x.Whole();
    const int16_t by = s.ball.pos.y.Whole();
    e.pos.x = Fix::FromInt(static_cast<int16_t>(bx < kCentreSpotX ? bx - 3 : bx + 3));
    e.pos.y = Fix::FromInt(by);
    e.dest_x = e.pos.x.Whole();
    e.dest_y = e.pos.y.Whole();
    e.delta = {};
    e.speed = 0;
    e.player_state = static_cast<uint8_t>(PlayerState::ThrowIn);
    // Re-park every tick — keep stick aim; default face into pitch.
    const int8_t into =
        (bx < kCentreSpotX) ? static_cast<int8_t>(2) : static_cast<int8_t>(6);
    int8_t face = into;
    if (tc.current_allowed_direction >= 0 && tc.current_allowed_direction <= 7)
        face = ClampDirToTurnFlags(
            static_cast<int8_t>(tc.current_allowed_direction),
            s.globals.player_turn_flags);
    if (face < 0) face = into;
    e.direction = face;
    e.player_direction = face;
    tc.direction = face;
    tc.controlled_pl_direction = face;
    tc.current_allowed_direction = face;
    s.globals.hide_ball = 1;
}

inline void PlaceTakerNearSpot(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    int slot = tc.controlled_slot;
    if (slot < 0 || slot >= kPitchPlayers ||
        s.players[static_cast<size_t>(slot)].team_number != side + 1) {
        slot = side * 11 + 1; // first outfield
        tc.controlled_slot = static_cast<int8_t>(slot);
    }
    Entity& e = s.players[static_cast<size_t>(slot)];
    e.pos.x = Fix::FromInt(s.globals.foul_x);
    e.pos.y = Fix::FromInt(s.globals.foul_y);
    e.dest_x = s.globals.foul_x;
    e.dest_y = s.globals.foul_y;
    e.delta = {};
    e.speed = 0;
}

inline void CompleteOopRestart(MatchState& s, GameState gs, bool is_goal) {
    if (is_goal) {
        // Goal: brief walk-on stoppage, then UpdateTime resumes InProgress.
        // (Full kick-off take / walk-on animation is later; ball returns to centre.)
        (void)gs;
        SetGameState(s, GameState::PlayersToInitialPositions);
        SetPl(s, GameStatePl::Stopped);
        PlaceBallAtCentre(s);
        s.globals.foul_x = kCentreSpotX;
        s.globals.foul_y = kCentreSpotY;
        s.globals.player_turn_flags = kTurnFlagsAll;
        s.globals.break_camera_mode = kBreakCameraModeRestart;
        s.clock.stoppage_event_timer = 50;
        ClearSpinTimers(s);
        for (int i = 0; i < 2; ++i)
            s.sides[static_cast<size_t>(i)].control.ball_in_play = 0;
        return;
    }
    const int16_t sx = s.globals.foul_x;
    const int16_t sy = s.globals.foul_y;
    const uint8_t taking = TakingTeamForOop(s, gs, false);
    const uint8_t flags = TurnFlagsForOop(gs, sx, sy);
    const uint8_t cam = CameraForOop(gs, sy);
    BeginRestart(s, gs, sx, sy, flags, cam, taking);

    const int side = static_cast<int>(taking) - 1;
    if (side < 0 || side > 1) return;
    PickRestartTaker(s, side);
    if (IsThrowInState(gs))
        PlaceThrowInTaker(s, side);
    else
        PlaceTakerNearSpot(s, side);
}

inline const std::array<Dest, 8>& AimTableForState(const MatchState& s) {
    const GameState gs = GetGameState(s);
    if (IsThrowInState(gs)) {
        return (s.globals.foul_x <= kCentreSpotX) ? kLeftThrowInBallDestDelta
                                                  : kRightThrowInBallDestDelta;
    }
    if (gs == GameState::Penalty || gs == GameState::Penalties)
        return kPenaltyBallDestDelta;
    if (IsCornerState(gs)) {
        const bool lower = s.globals.foul_y > kCentreSpotY;
        const bool right = s.globals.foul_x > kCentreSpotX;
        if (!lower && !right) return kUpperLeftCornerBallDestDelta;
        if (!lower && right) return kUpperRightCornerBallDestDelta;
        if (lower && !right) return kLowerLeftCornerBallDestDelta;
        return kLowerRightCornerBallDestDelta;
    }
    return kDefaultDestinations;
}

inline Dest AimDestForDir(const MatchState& s, int dir) {
    if (dir < 0 || dir > 7) dir = 0;
    return AimTableForState(s)[static_cast<size_t>(dir)];
}

inline void ResumeOpenPlay(MatchState& s) {
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.globals.player_turn_flags = kTurnFlagsAll;
    s.globals.hide_ball = 0;
    s.phase = MatchPhase::InPlay;
    for (int i = 0; i < 2; ++i) {
        TeamControl& tc = s.sides[static_cast<size_t>(i)].control;
        tc.ball_in_play = 1;
        if (tc.long_pass != 0) tc.long_pass = 0;
    }
    MarkBallLoose(s);
}

// Throw-in / free kick: after idle timeout, park a teammate ahead of the
// taker's face as the tap-pass target (original shortfall assist).
inline void ApproachRestartReceiver(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    const int taker = tc.controlled_slot;
    if (taker < 0 || taker >= kPitchPlayers) return;

    int face = tc.current_allowed_direction;
    if (face < 0 || face > 7)
        face = s.players[static_cast<size_t>(taker)].direction;
    face = ClampDirToTurnFlags(static_cast<int8_t>(face),
                               s.globals.player_turn_flags);
    if (face < 0 || face > 7) face = (s.globals.foul_x < kCentreSpotX) ? 2 : 6;

    const Entity& th = s.players[static_cast<size_t>(taker)];
    const Dest off = kDefaultDestinations[static_cast<size_t>(face)];
    const int16_t ax = static_cast<int16_t>(
        th.pos.x.Whole() + off.x / (1000 / kRestartApproachUnits));
    const int16_t ay = static_cast<int16_t>(
        th.pos.y.Whole() + off.y / (1000 / kRestartApproachUnits));

    const int base = side * 11;
    int target = tc.pass_to_slot;
    if (target < 0 || target >= kPitchPlayers || target == taker) {
        int32_t best_d = 0x7fffffff;
        target = -1;
        for (int i = 1; i < 11; ++i) {
            const int slot = base + i;
            if (slot == taker) continue;
            Entity& e = s.players[static_cast<size_t>(slot)];
            if (e.cards < 0) continue;
            const int32_t ddx = e.pos.x.Whole() - ax;
            const int32_t ddy = e.pos.y.Whole() - ay;
            const int32_t d = ddx * ddx + ddy * ddy;
            if (d < best_d) {
                best_d = d;
                target = slot;
            }
        }
    }
    if (target < 0) return;

    Entity& recv = s.players[static_cast<size_t>(target)];
    recv.dest_x = ax;
    recv.dest_y = ay;
    tc.pass_to_slot = static_cast<int8_t>(target);
    tc.long_pass = -1; // shortfall active: keep approaching
}

inline void TickRestartShortfall(MatchState& s, int side) {
    if (GetPl(s) == GameStatePl::InProgress) return;
    if (!IsShortfallAssistState(GetGameState(s))) return;
    if (s.globals.last_team_played_before_break !=
        static_cast<uint8_t>(side + 1))
        return;

    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    if (tc.long_pass > 0) {
        --tc.long_pass;
        if (tc.long_pass == 0) ApproachRestartReceiver(s, side);
    } else if (tc.long_pass < 0) {
        ApproachRestartReceiver(s, side);
    }
}

inline bool IsLastManFoul(const MatchState& s, int offending_side, int victim_slot) {
    // Fouled player closer to offender's goal than any offender outfield mate.
    const int16_t goal_y =
        (s.globals.team_playing_up == static_cast<uint8_t>(offending_side + 1))
            ? kPlayableMaxY
            : kPlayableMinY;
    const Entity& victim = s.players[static_cast<size_t>(victim_slot)];
    const int32_t vdx = victim.pos.x.Whole() - kCentreSpotX;
    const int32_t vdy = victim.pos.y.Whole() - goal_y;
    const int32_t vdist = vdx * vdx + vdy * vdy;

    const int base = offending_side * 11;
    for (int i = 1; i < 11; ++i) {
        const Entity& e = s.players[static_cast<size_t>(base + i)];
        if (e.cards < 0) continue;
        const int32_t dx = e.pos.x.Whole() - kCentreSpotX;
        const int32_t dy = e.pos.y.Whole() - goal_y;
        if (dx * dx + dy * dy < vdist) return false;
    }
    return true;
}

// Returns which_card value set (0 = none).
inline uint8_t RollCardForFoul(MatchState& s, int offending_side, int tackler_slot,
                               int victim_slot, bool in_penalty_area) {
    (void)in_penalty_area;
    // Strictness gate: ((tick & 0x1E) >> 1) >= chance → no card. Use mid table.
    constexpr int kChance = 3; // ~7.5 min mid
    if (static_cast<int>((s.tick & 0x1Eu) >> 1) >= kChance) return 0;

    Entity& tackler = s.players[static_cast<size_t>(tackler_slot)];
    const bool last_man = IsLastManFoul(s, offending_side, victim_slot);
    const uint8_t roll = s.resolve_rng.Draw();
    const bool rare = roll < 32; // 12.5 %

    uint8_t card = 0;
    if (last_man)
        card = rare ? 1 : 2; // yellow rare, else red
    else
        card = rare ? 2 : 1; // red rare, else yellow

    if (tackler.cards >= 1 && card == 1) card = 3; // second yellow → red path

    if (card == 2 || card == 3) {
        tackler.cards = -1;
        tackler.sent_away = 1;
        s.sides[static_cast<size_t>(offending_side)].stats.sendings_off += 1;
    } else {
        tackler.cards = static_cast<int16_t>(tackler.cards + 1);
        s.sides[static_cast<size_t>(offending_side)].stats.bookings += 1;
    }

    s.globals.which_card = card;
    s.globals.booked_player = static_cast<int8_t>(tackler_slot);
    s.globals.last_team_booked = static_cast<uint8_t>(offending_side + 1);
    {
        const uint8_t sq = static_cast<uint8_t>(
            (tackler_slot >= 0 && tackler_slot < kPitchPlayers)
                ? (tackler_slot % 11)
                : 0);
        AppendChronicle(s,
                        (card == 2 || card == 3) ? MatchEventKind::Red
                                                 : MatchEventKind::Yellow,
                        static_cast<uint8_t>(offending_side), sq);
    }
    ActivateReferee(s);
    return card;
}

inline void RollInjuryOnTackle(MatchState& s, int victim_side, int victim_slot) {
    if ((s.resolve_rng.Draw() & 3) != 0) return; // 25 % considered
    if (CountInjuredOnSide(s, victim_side) >= kMaxInjuriesPerSide) return;

    Entity& v = s.players[static_cast<size_t>(victim_slot)];
    constexpr std::array<int, 4> kProb = {48, 28, 20, 14};
    constexpr std::array<int, 4> kProbInj = {96, 57, 41, 28};
    const int len = s.clock.game_length > 3 ? 3 : s.clock.game_length;
    const int thr = (v.injury_level > 0) ? kProbInj[static_cast<size_t>(len)]
                                         : kProb[static_cast<size_t>(len)];
    if (static_cast<int>(s.resolve_rng.Draw()) >= thr) return;

    constexpr std::array<int16_t, 7> kLevels = {42, 7, 5, 4, 3, 2, 1};
    const int idx = static_cast<int>(s.resolve_rng.Draw() % 7);
    v.injury_level = static_cast<int16_t>(v.injury_level + kLevels[static_cast<size_t>(idx)]);
    if (victim_slot >= 0 && victim_slot < kPitchPlayers) {
        const uint8_t sq = static_cast<uint8_t>(victim_slot % 11);
        s.sides[static_cast<size_t>(victim_side)].squad[static_cast<size_t>(sq)]
            .is_injured = 1;
        AppendChronicle(s, MatchEventKind::Injury,
                        static_cast<uint8_t>(victim_side), sq);
    }
}

inline void ApplyFoulConsequences(MatchState& s, int offending_side, int tackler_slot,
                                  int victim_slot) {
    Entity& victim = s.players[static_cast<size_t>(victim_slot)];
    const int16_t vx = victim.pos.x.Whole();
    const int16_t vy = victim.pos.y.Whole();
    const bool in_pen = vx >= kPenBoxXMin && vx <= kPenBoxXMax &&
                        (vy <= kPenaltyBoxTopY || vy >= kPenaltyBoxBotY);

    {
        const int sq = SquadIndexFromPitchSlot(tackler_slot);
        if (PlayerMatchStats* st = MatchStatsFor(s, offending_side, sq))
            BumpU16(st->fouls_conceded);
    }

    RollInjuryOnTackle(s, 1 - offending_side, victim_slot);
    RollCardForFoul(s, offending_side, tackler_slot, victim_slot, in_pen);
    ClassifyAndBeginFoulRestart(s, vx, vy, offending_side);
    const int take_side = 1 - offending_side;
    PickRestartTaker(s, take_side);
    PlaceTakerNearSpot(s, take_side);
}

// Restart take: kick or throw while Stopped. Returns true if launched.
inline bool ApplyRestartTake(MatchState& s, int side) {
    if (GetPl(s) == GameStatePl::InProgress) return false;
    const GameState gs = GetGameState(s);
    if (!IsRestartTakeState(gs)) return false;

    const uint8_t taking = s.globals.last_team_played_before_break;
    if (taking != static_cast<uint8_t>(side + 1)) return false;

    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    // Wait for tap/hold classification — do not fire on button-down.
    const bool is_pass = tc.quick_fire != 0;
    const bool is_kick = tc.normal_fire != 0;
    if (!is_pass && !is_kick) return false;
    if (tc.controlled_slot < 0 || tc.controlled_slot >= kPitchPlayers) return false;

    Entity& kicker = s.players[static_cast<size_t>(tc.controlled_slot)];
    // Must be near the restart spot.
    const int32_t dx = kicker.pos.x.Whole() - s.globals.foul_x;
    const int32_t dy = kicker.pos.y.Whole() - s.globals.foul_y;
    if (dx * dx + dy * dy > 72) return false;

    int dir = tc.current_allowed_direction;
    dir = ClampDirToTurnFlags(static_cast<int8_t>(dir), s.globals.player_turn_flags);
    if (dir < 0 || dir > 7) dir = kicker.direction;
    if (dir < 0 || dir > 7) dir = 0;

    Entity& ball = s.ball;
    const bool throw_in = IsThrowInState(gs);
    const bool shortfall = IsShortfallAssistState(gs);
    const int16_t bx = ball.pos.x.Whole();
    const int16_t by = ball.pos.y.Whole();

    if (shortfall && is_pass) {
        // Tap: assisted pass to pass_to (or short kick/throw in facing dir).
        if (tc.pass_to_slot >= 0 && tc.pass_to_slot < kPitchPlayers &&
            tc.pass_to_slot != tc.controlled_slot) {
            const Entity& recv =
                s.players[static_cast<size_t>(tc.pass_to_slot)];
            ball.dest_x = recv.pos.x.Whole();
            ball.dest_y = recv.pos.y.Whole();
            tc.pass_in_progress = 1;
        } else {
            const Dest off = kDefaultDestinations[static_cast<size_t>(dir)];
            ball.dest_x = static_cast<int16_t>(bx + off.x / 2);
            ball.dest_y = static_cast<int16_t>(by + off.y / 2);
            tc.pass_to_slot = -1;
            tc.pass_in_progress = 0;
        }
        ball.speed = throw_in ? kRestartThrowPassSpeed : kRestartKickPassSpeed;
        ball.delta.z = Fix::FromRaw(throw_in ? kRestartThrowPassDeltaZRaw
                                             : kRestartKickPassDeltaZRaw);
    } else {
        const Dest off = AimDestForDir(s, dir);
        ball.dest_x = static_cast<int16_t>(bx + off.x);
        ball.dest_y = static_cast<int16_t>(by + off.y);
        tc.pass_to_slot = -1;
        tc.pass_in_progress = 0;
        ball.speed = throw_in ? kRestartThrowSpeed : kRestartKickSpeed;
        ball.delta.z = Fix::FromRaw(throw_in ? kRestartThrowDeltaZRaw
                                             : kRestartKickDeltaZRaw);
    }
    ball.direction = static_cast<int16_t>(dir);
    ball.pos.z = Fix{};

    {
        const auto ks = static_cast<PlayerState>(kicker.player_state);
        if (ks == PlayerState::ThrowIn || ks == PlayerState::GoalieClaimed ||
            ks == PlayerState::GoalieCatchingBall)
            kicker.player_state = static_cast<uint8_t>(PlayerState::Normal);
    }

    tc.quick_fire = 0;
    tc.normal_fire = 0;
    tc.player_has_ball = 0;
    tc.long_pass = 0;
    tc.pass_kick_timer = kRestartLockoutTicks;
    tc.ball_can_be_controlled = 0;
    s.clock.last_team_played = static_cast<uint8_t>(side + 1);

    ClearSpinTimers(s);
    if (!throw_in) tc.spin_timer = 0;
    ResumeOpenPlay(s);
    return true;
}

} // namespace at
