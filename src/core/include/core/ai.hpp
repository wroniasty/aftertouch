#pragma once
#include "core/angle.hpp"
#include "core/goalkeeper.hpp"
#include "core/match_clock.hpp"
#include "core/match_input.hpp"
#include "core/match_state.hpp"
#include "core/possession.hpp"
#include "core/set_pieces.hpp"
#include "core/trig.hpp"

#include <cstdint>

// CPU brain + selection polish — doc/implementation/B9-ai.md / doc/AI.md.

namespace at {

// Alias of possession.hpp kPassArrivalSwitchTicks (AI.md §2.3).
inline constexpr int16_t kPlayerSwitchTimerTicks = kPassArrivalSwitchTicks;
inline constexpr int32_t kShootMaxDistSq         = 28800;
inline constexpr int32_t kShootMidDistSq         = 12800;
inline constexpr int32_t kShootCloseDistSq       = 3200;
inline constexpr int32_t kPassPressureCloseSq    = 800;
inline constexpr int32_t kPassPressureNearSq     = 5000;
inline constexpr int32_t kNoPassNearGoalSq       = 9800;
inline constexpr int32_t kChaseSwapMarginSq      = 50;
inline constexpr int32_t kChaseLockDistSq        = 800;

inline int32_t SquaredXY(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    const int32_t dx = static_cast<int32_t>(x0) - x1;
    const int32_t dy = static_cast<int32_t>(y0) - y1;
    return dx * dx + dy * dy;
}

inline Dest OppGoalCentre(const MatchState& s, int side) {
    // Top team (playing up) attacks bottom (high y).
    const uint8_t team = static_cast<uint8_t>(side + 1);
    Dest g;
    g.x = kCentreSpotX;
    g.y = (s.globals.team_playing_up == team) ? kPlayableMaxY : kPlayableMinY;
    return g;
}

inline bool AttacksDown(const MatchState& s, int side) {
    return s.globals.team_playing_up == static_cast<uint8_t>(side + 1);
}

inline int AngleErrorOctant(int facing_oct, int16_t fine_heading) {
    // facing*32 vs fine heading (0..255); return signed error in heading units.
    const int face_h = facing_oct * 32;
    int err = face_h - static_cast<int>(fine_heading);
    if (err > 128) err -= 256;
    if (err < -128) err += 256;
    return err;
}

inline bool IsEligibleAi(const Entity& e, const TeamControl& tc, int slot,
                         int exclude_a, int exclude_b) {
    if (slot == exclude_a || slot == exclude_b) return false;
    if (e.cards < 0) return false;
    if (e.player_ordinal == 1 && !tc.goalie_playing_or_out) return false;
    const auto st = static_cast<PlayerState>(e.player_state);
    if (st == PlayerState::Tackling || st == PlayerState::Tackled ||
        st == PlayerState::JumpHeader || st == PlayerState::StaticHeader ||
        st == PlayerState::Injured)
        return false;
    return true;
}

inline int FindPassConeTeammate(MatchState& s, int side, int controlled, int face);

inline void UpdatePlayerBeingPassedTo(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    if (tc.player_switch_timer > 0) {
        --tc.player_switch_timer;
        return;
    }
    // Open play: ballInPlay. Throw-in / free kick: pass assist while aiming.
    const bool restart_pass_assist =
        GetPl(s) != GameStatePl::InProgress &&
        IsShortfallAssistState(GetGameState(s)) &&
        s.globals.last_team_played_before_break ==
            static_cast<uint8_t>(side + 1);
    if (GetPl(s) != GameStatePl::InProgress && !tc.ball_in_play &&
        !restart_pass_assist)
        return;
    // Leave an in-flight pass target alone (AI.md §2.2).
    if (tc.pass_in_progress && tc.pass_to_slot >= 0) return;
    // Shortfall receiver stays sticky once summoned.
    if (tc.long_pass < 0 && tc.pass_to_slot >= 0) return;

    const int base = side * 11;
    const int controlled = tc.controlled_slot;
    const int kicking = tc.passing_kicking_slot;

    for (int i = 0; i < 11; ++i) {
        Entity& e = s.players[static_cast<size_t>(base + i)];
        e.ball_distance =
            SquaredXY(e.pos.x.Whole(), e.pos.y.Whole(), s.ball.pos.x.Whole(),
                      s.ball.pos.y.Whole());
    }

    int face = tc.current_allowed_direction;
    if ((face < 0 || face > 7) && controlled >= 0 && controlled < kPitchPlayers)
        face = s.players[static_cast<size_t>(controlled)].direction;

    int best = -1;
    if (face >= 0 && face <= 7 && controlled >= 0 && controlled < kPitchPlayers)
        best = FindPassConeTeammate(s, side, controlled, face);

    if (best < 0) {
        int32_t best_d = 0x7fffffff;
        for (int i = 0; i < 11; ++i) {
            const int slot = base + i;
            Entity& e = s.players[static_cast<size_t>(slot)];
            if (!IsEligibleAi(e, tc, slot, controlled, kicking)) continue;
            if (e.ball_distance < best_d) {
                best_d = e.ball_distance;
                best = slot;
            }
        }
    }
    tc.pass_to_slot = static_cast<int8_t>(best);
}

// Apply pass/kicker exclusions + switch lockout on top of UpdateControlledPlayer.
inline void RefineControlledSelection(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    if (!tc.ball_out_of_play) return;
    if (tc.player_switch_timer > 0) return;

    const int base = side * 11;
    const int exclude_pass = tc.pass_to_slot;
    const int exclude_kick = tc.passing_kicking_slot;

    int best = -1;
    int32_t best_d = 0x7fffffff;
    for (int i = 0; i < 11; ++i) {
        const int slot = base + i;
        Entity& e = s.players[static_cast<size_t>(slot)];
        if (!IsEligibleAi(e, tc, slot, exclude_pass, exclude_kick)) continue;
        if (e.ball_distance < best_d) {
            best_d = e.ball_distance;
            best = slot;
        }
    }
    if (best < 0 || best == tc.controlled_slot) return;

    if (tc.controlled_slot >= 0 && tc.controlled_slot < kPitchPlayers) {
        Entity& out = s.players[static_cast<size_t>(tc.controlled_slot)];
        out.dest_x = out.pos.x.Whole();
        out.dest_y = out.pos.y.Whole();
        out.delta = {};
    }
    tc.controlled_slot = static_cast<int8_t>(best);
}

// Selection + park at spot (human + CPU). Throw-ins keep their offset pose.
inline void PickCpuRestartTaker(MatchState& s, int side) {
    PickRestartTaker(s, side);
    if (GetPl(s) == GameStatePl::InProgress) return;
    if (!IsRestartTakeState(GetGameState(s))) return;
    if (s.globals.last_team_played_before_break !=
        static_cast<uint8_t>(side + 1))
        return;
    if (IsThrowInState(GetGameState(s)))
        PlaceThrowInTaker(s, side);
    else
        PlaceTakerNearSpot(s, side);
}

inline int FindPassConeTeammate(MatchState& s, int side, int controlled, int face) {
    // Closest teammate in ±1 octant of the owner's facing (owner pos, else ball).
    int16_t ox = s.ball.pos.x.Whole();
    int16_t oy = s.ball.pos.y.Whole();
    if (controlled >= 0 && controlled < kPitchPlayers) {
        ox = s.players[static_cast<size_t>(controlled)].pos.x.Whole();
        oy = s.players[static_cast<size_t>(controlled)].pos.y.Whole();
    }
    const int team = side + 1;
    const int base = side * 11;
    int best = -1;
    int32_t best_d = 0x7fffffff;
    for (int i = 0; i < 11; ++i) {
        const int slot = base + i;
        if (slot == controlled) continue;
        Entity& e = s.players[static_cast<size_t>(slot)];
        if (e.cards < 0 || e.team_number != team) continue;
        const auto st = static_cast<PlayerState>(e.player_state);
        if (st != PlayerState::Normal && st != PlayerState::Unknown) continue;
        const int16_t h =
            AngleTo(Dest{ox, oy}, Dest{e.pos.x.Whole(), e.pos.y.Whole()});
        if (h < 0) continue;
        const int oct = static_cast<int>(((static_cast<uint16_t>(h) + 16u) & 0xFFu) >> 5);
        int d = oct - face;
        if (d < 0) d += 8;
        if (d > 4) d = 8 - d;
        if (d > 1) continue;
        const int32_t dist = SquaredXY(e.pos.x.Whole(), e.pos.y.Whole(), ox, oy);
        if (dist < best_d) {
            best_d = dist;
            best = slot;
        }
    }
    return best;
}

inline void AI_SetControlsDirection(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    tc.current_allowed_direction = -1;
    tc.direction = -1;
    tc.quick_fire = 0;
    tc.normal_fire = 0;
    tc.fire_pressed = 0;
    tc.fire_this_frame = 0;

    if (tc.controlled_slot < 0 || tc.controlled_slot >= kPitchPlayers) return;
    Entity& pl = s.players[static_cast<size_t>(tc.controlled_slot)];

    // Aftertouch window: steer from ai_ball_spin_direction.
    if (tc.spin_timer >= 0 && tc.spin_timer < 10) {
        int kick_dir = tc.controlled_pl_direction;
        if (kick_dir < 0 || kick_dir > 7) kick_dir = pl.direction;
        int adj = 0;
        if (tc.ai_ball_spin_direction < 0) adj = -1;
        else if (tc.ai_ball_spin_direction > 0) adj = 1;
        if (tc.ai_aftertouch_strength > 0)
            adj *= (tc.ai_aftertouch_strength > 2 ? 2 : tc.ai_aftertouch_strength);
        tc.current_allowed_direction =
            static_cast<int16_t>((kick_dir + adj + 8) & 7);
        tc.direction = tc.current_allowed_direction;
        return;
    }

    // Restart take.
    if (GetPl(s) != GameStatePl::InProgress &&
        IsRestartTakeState(GetGameState(s)) &&
        s.globals.last_team_played_before_break ==
            static_cast<uint8_t>(side + 1)) {
        const Dest goal = OppGoalCentre(s, side);
        const int16_t h = AngleTo(
            Dest{s.ball.pos.x.Whole(), s.ball.pos.y.Whole()}, goal);
        int dir = (h < 0) ? 0
                          : static_cast<int>(
                                ((static_cast<uint16_t>(h) + 16u) & 0xFFu) >> 5);
        dir = ClampDirToTurnFlags(static_cast<int8_t>(dir),
                                  s.globals.player_turn_flags);
        tc.current_allowed_direction = static_cast<int16_t>(dir);
        tc.direction = tc.current_allowed_direction;
        tc.fire_this_frame = 1;
        tc.quick_fire = 1;
        tc.fire_pressed = 1;
        return;
    }

    if (GetPl(s) != GameStatePl::InProgress) return;

    const uint8_t ai_rand = s.gameplay_rng.Draw();
    const Dest goal = OppGoalCentre(s, side);
    const int32_t d6 = SquaredXY(s.ball.pos.x.Whole(), s.ball.pos.y.Whole(),
                                 goal.x, goal.y);
    const int16_t d5 =
        AngleTo(Dest{s.ball.pos.x.Whole(), s.ball.pos.y.Whole()}, goal);
    int face = pl.direction;
    if (face < 0 || face > 7) face = 0;

    // Bands already refreshed in ApplyTeamControls before the brain runs.
    const bool near = pl.ball_distance <= kDistCloseSq;
    const bool very = pl.ball_distance <= kDistVeryCloseSq;

    // Header / tackle trigger (simplified AI.md §5.7).
    if (pl.player_ordinal != 1 && pl.ball_distance <= 648) {
        const int z = s.ball.pos.z.Whole();
        const bool height_ok =
            (z >= 8 && z <= 14) ||
            (s.ball.delta.z.Raw() < 0 && z >= 12 && z <= 20);
        if (height_ok) {
            const int16_t to_ball = AngleTo(
                Dest{pl.pos.x.Whole(), pl.pos.y.Whole()},
                Dest{s.ball.pos.x.Whole(), s.ball.pos.y.Whole()});
            if (to_ball >= 0) {
                const int oct = static_cast<int>(
                    ((static_cast<uint16_t>(to_ball) + 16u) & 0xFFu) >> 5);
                if (oct == face) {
                    tc.fire_this_frame = 1;
                    tc.fire_pressed = 1;
                    tc.current_allowed_direction = static_cast<int16_t>(oct);
                    tc.direction = tc.current_allowed_direction;
                    tc.ai_timer = 15;
                    return;
                }
            }
        }
    }

    // On-ball decisions.
    if (near || very || tc.player_has_ball) {
        // Shoot.
        bool can_shoot = d6 <= kShootMaxDistSq;
        if (can_shoot && d6 >= kShootMidDistSq)
            can_shoot = (ai_rand & 3) == 0;
        if (can_shoot && d5 >= 0) {
            const int tol = (d6 <= kShootCloseDistSq) ? 0x32 : 0x0F;
            const int err = AngleErrorOctant(face, d5);
            if (err <= tol && err >= -tol) {
                tc.current_allowed_direction = static_cast<int16_t>(face);
                tc.direction = static_cast<int16_t>(face);
                tc.normal_fire = 1;
                tc.fire_pressed = 1;
                tc.fire_this_frame = 1;
                tc.ai_ball_spin_direction =
                    static_cast<int16_t>(err >= 0 ? -1 : 1);
                tc.ai_aftertouch_strength = static_cast<int16_t>((ai_rand % 3));
                return;
            }
        }

        // Pass under pressure.
        const int opp = 1 - side;
        const TeamControl& otc = s.sides[static_cast<size_t>(opp)].control;
        int32_t opp_d = 0x7fffffff;
        if (otc.controlled_slot >= 0 && otc.controlled_slot < kPitchPlayers)
            opp_d = s.players[static_cast<size_t>(otc.controlled_slot)]
                        .ball_distance;
        bool want_pass = false;
        if (opp_d < kPassPressureCloseSq) want_pass = true;
        else if (opp_d < kPassPressureNearSq)
            want_pass = (ai_rand <= 8) && ((s.tick & 0x0Cu) == 0);
        if (want_pass && d6 >= kNoPassNearGoalSq) {
            const int target = FindPassConeTeammate(s, side, tc.controlled_slot, face);
            if (target >= 0) {
                tc.current_allowed_direction = static_cast<int16_t>(face);
                tc.direction = static_cast<int16_t>(face);
                tc.quick_fire = 1;
                tc.fire_pressed = 1;
                tc.fire_this_frame = 1;
                return;
            }
            // Jink.
            const int jink = ((s.tick & 0x80u) != 0) ? 1 : -1;
            tc.current_allowed_direction =
                static_cast<int16_t>((face + jink + 8) & 7);
            tc.direction = tc.current_allowed_direction;
            return;
        }

        // Dribble toward goal.
        if (d5 >= 0) {
            const int oct = static_cast<int>(
                ((static_cast<uint16_t>(d5) + 16u) & 0xFFu) >> 5);
            tc.current_allowed_direction = static_cast<int16_t>(oct);
            tc.direction = tc.current_allowed_direction;
        }
        return;
    }

    // Chase: swap to pass target if much closer.
    if (tc.pass_to_slot >= 0 && tc.pass_to_slot < kPitchPlayers) {
        const Entity& recv = s.players[static_cast<size_t>(tc.pass_to_slot)];
        if (recv.ball_distance + kChaseSwapMarginSq < pl.ball_distance) {
            tc.controlled_slot = tc.pass_to_slot;
            // fall through with new controlled — re-fetch
        }
    }
    Entity& chaser = s.players[static_cast<size_t>(tc.controlled_slot)];
    const int16_t to_ball = AngleTo(
        Dest{chaser.pos.x.Whole(), chaser.pos.y.Whole()},
        Dest{s.ball.pos.x.Whole(), s.ball.pos.y.Whole()});
    int chase_dir = chaser.direction;
    const bool reaim = chaser.ball_distance < kChaseLockDistSq ||
                       ((s.tick & 0x0Eu) == 0);
    if (reaim && to_ball >= 0)
        chase_dir = static_cast<int>(
            ((static_cast<uint16_t>(to_ball) + 16u) & 0xFFu) >> 5);
    // CPU-vs-CPU wobble.
    if (s.sides[static_cast<size_t>(1 - side)].control.player_number == 0 &&
        (ai_rand & 0x0E) == 0) {
        chase_dir = (chase_dir + ((ai_rand & 1) ? 1 : -1) + 8) & 7;
    }
    tc.current_allowed_direction = static_cast<int16_t>(chase_dir);
    tc.direction = tc.current_allowed_direction;

    // Challenge carrier.
    {
        const int opp_side = 1 - side;
        const TeamControl& otc = s.sides[static_cast<size_t>(opp_side)].control;
        if (otc.player_has_ball && chaser.ball_distance <= 200 &&
            otc.controlled_slot >= 0) {
            const Entity& carrier =
                s.players[static_cast<size_t>(otc.controlled_slot)];
            int dd = chaser.direction - carrier.direction;
            if (dd < 0) dd = -dd;
            if (dd > 4) dd = 8 - dd;
            if (dd > 1) {
                tc.fire_this_frame = 1;
                tc.fire_pressed = 1;
                tc.normal_fire = 1;
            }
        }
    }
}

} // namespace at
