#pragma once
#include "core/aftertouch.hpp" // ResetBothSpinTimers on capture
#include "core/angle.hpp"
#include "core/match_clock.hpp"
#include "core/match_state.hpp"

#include <array>
#include <cstdint>

// Possession / dribble — doc/implementation/B5-possession.md / doc/CONTROL.md.
// Band metric correction: doc/MOVEMENT.md §6.
// Does not include movement.hpp (called from ApplyTeamControls).

namespace at {

// Planar squared-distance thresholds (MOVEMENT §6).
inline constexpr int32_t kDistVeryCloseSq = 32;    // ~5.7 u
inline constexpr int32_t kDistCloseSq     = 72;    // ~8.5 u
inline constexpr int32_t kDistNotFarSq    = 2450;  // ~49.5 u

inline constexpr int16_t kBallZBand4  = 4;
inline constexpr int16_t kBallZBand8  = 8;
inline constexpr int16_t kBallZBand12 = 12;
inline constexpr int16_t kBallZBand17 = 17;

inline constexpr std::array<int16_t, 8> kBallSpeedDeltaWhenControlled = {
    130, 116, 102, 88, 74, 60, 46, 32};

// [CANDIDATE: amiga unk_1106EA asm:34791] — B13 / R4.
// Direction changes a carrier may make before the ball gets away from him,
// indexed by Ball Control. Note the accelerating spacing: the top of the scale
// is worth far more than the bottom, and a Control-7 player can turn more than
// five times as often as a Control-0 player. CONTROL.md §4 had losing the ball
// happening only through a tackle; this is the other way, and the ledger calls
// it the single clearest attribute effect in the game.
inline constexpr std::array<int16_t, 8> kDribbleTouchesAllowed = {
    4, 5, 6, 8, 11, 14, 17, 21};

static_assert(kDribbleTouchesAllowed.size() == kAttrTableSize);

// Possession interruption after the touch count overflows (amiga asm:35314).
inline constexpr int16_t kTouchOverflowWonBallTicks = 8;

inline constexpr std::array<Dest, 8> kBallPlOffsets = {{
    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
}};

// B13 / R2 invariant: one entry per attribute value.
static_assert(kBallSpeedDeltaWhenControlled.size() == kAttrTableSize);

// Named for the call site; the range lives in match_state.hpp (B13 / R2).
inline int ControlAttrIndex(uint8_t attr) { return AttrIndex0to7(attr); }

inline int32_t PossessionBallDistSq(const Entity& e, const Entity& ball) {
    const int32_t dx = static_cast<int32_t>(e.pos.x.Whole()) -
                       static_cast<int32_t>(ball.pos.x.Whole());
    const int32_t dy = static_cast<int32_t>(e.pos.y.Whole()) -
                       static_cast<int32_t>(ball.pos.y.Whole());
    return dx * dx + dy * dy;
}

inline bool PossessionSpecialState(const Entity& e) {
    const auto st = static_cast<PlayerState>(e.player_state);
    return st == PlayerState::Tackling || st == PlayerState::Tackled ||
           st == PlayerState::JumpHeader || st == PlayerState::StaticHeader ||
           st == PlayerState::Injured;
}

inline void ClassifyHeightBands(TeamControl& tc, int16_t z_whole) {
    tc.ball_less_equal_4 = 0;
    tc.ball_4_to_8 = 0;
    tc.ball_8_to_12 = 0;
    tc.ball_12_to_17 = 0;
    tc.ball_above_17 = 0;
    if (z_whole <= kBallZBand4)
        tc.ball_less_equal_4 = 1;
    else if (z_whole <= kBallZBand8)
        tc.ball_4_to_8 = 1;
    else if (z_whole <= kBallZBand12)
        tc.ball_8_to_12 = 1;
    else if (z_whole <= kBallZBand17)
        tc.ball_12_to_17 = 1;
    else
        tc.ball_above_17 = 1;
}

inline void ClassifyPlanarBands(TeamControl& tc, int32_t dist_sq) {
    tc.prev_pl_very_close_to_ball = tc.pl_very_close_to_ball;
    tc.pl_very_close_to_ball =
        static_cast<uint8_t>(dist_sq <= kDistVeryCloseSq ? 1 : 0);
    tc.pl_close_to_ball = static_cast<uint8_t>(dist_sq <= kDistCloseSq ? 1 : 0);
    tc.pl_not_far_from_ball =
        static_cast<uint8_t>(dist_sq <= kDistNotFarSq ? 1 : 0);
}

inline void UpdateProximityBands(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    const int base = side * 11;

    for (int i = 0; i < 11; ++i) {
        Entity& e = s.players[static_cast<size_t>(base + i)];
        e.ball_distance = PossessionBallDistSq(e, s.ball);
    }

    ClassifyHeightBands(tc, s.ball.pos.z.Whole());

    if (tc.controlled_slot < 0 || tc.controlled_slot >= kPitchPlayers) {
        tc.pl_very_close_to_ball = 0;
        tc.pl_close_to_ball = 0;
        tc.pl_not_far_from_ball = 0;
        return;
    }
    const Entity& ctrl = s.players[static_cast<size_t>(tc.controlled_slot)];
    ClassifyPlanarBands(tc, ctrl.ball_distance);
}

// Capture radius (squared) for a ball arriving at the designated receiver. Grows
// with ball speed so a fast pass cannot tunnel past the band between two of this
// side's frames. Speed is ~1/512 units per frame and this side is served every
// other frame, so the ball covers speed/256 units between checks; the band must
// cover at least half of that plus the resting band.
inline constexpr int32_t PassArrivalBandSq(int16_t ball_speed) {
    const int32_t step = (ball_speed > 0 ? ball_speed : 0) / 256;
    const int32_t r = 6 + step;
    const int32_t band = r * r;
    return band < kDistVeryCloseSq ? kDistVeryCloseSq : band;
}

inline void TickPassKickLockout(TeamControl& tc) {
    if (tc.pass_kick_timer > 0) {
        --tc.pass_kick_timer;
        if (tc.pass_kick_timer == 0) {
            tc.ball_can_be_controlled = 1;
            // Release the kicker.
            //
            // `passing_kicking_slot` was set on every kick and **never cleared**,
            // while three places use it as a hard exclusion: he cannot be
            // selected (UpdateControlledPlayer, RefineControlledSelection) and
            // cannot be passed to (UpdatePlayerBeingPassedTo). So the last man to
            // touch the ball was permanently unselectable — for the rest of the
            // match, unless a team-mate happened to kick.
            //
            // The visible symptom is "nobody closes on the ball": the player
            // standing over it is the one man control cannot go to, so it jumps
            // to a distant team-mate who then trudges across. In the C1B trace at
            // t=196 the ball is at (382,375) with slot 10 three units away and
            // slot 8 a hundred and twenty away — and slot 8 got it, because slot
            // 10 had just kicked.
            //
            // The exclusion exists to stop the kicker being re-selected on the
            // tick he kicks and to keep the pass-target and controlled selections
            // from colliding. Both are post-kick concerns, so it lives exactly as
            // long as the lockout that shares its purpose.
            tc.passing_kicking_slot = -1;
        }
    }
}

inline bool CanCapture(const MatchState& s, const TeamControl& tc, int slot) {
    if (GetPl(s) != GameStatePl::InProgress) return false;
    if (slot < 0 || slot >= kPitchPlayers) return false;

    // The post-kick lockout exists to stop the **kicker** re-capturing the ball
    // he has just struck. It was applied to the whole side, and since a side's
    // TeamControl is shared by all eleven players — and only ticks on that side's
    // frame, so 25 ticks is 50 real frames — it also forbade the *receiver* from
    // taking the pass for a full second. Any pass arriving sooner rolled straight
    // through him: the reported "the receiver doesn't control it, it bounces off".
    //
    // Scope it to the kicker. A team-mate may receive immediately; that is the
    // whole point of a pass.
    const bool is_kicker = (slot == tc.passing_kicking_slot);
    if (is_kicker) {
        if (!tc.ball_can_be_controlled) return false;
        if (tc.pass_kick_timer > 0) return false;
    }
    const Entity& e = s.players[static_cast<size_t>(slot)];
    if (PossessionSpecialState(e)) return false;
    if (e.ball_distance > kDistVeryCloseSq) return false;
    return true;
}

// AI.md §2.3: when the ball reaches the pass target, hand control over and
// arm the switch lockout. Does not clear pass_in_progress (capture credits).
inline constexpr int16_t kPassArrivalSwitchTicks = 25;

inline bool TryCompletePassArrival(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    if (GetPl(s) != GameStatePl::InProgress) return false;
    if (!tc.pass_in_progress) return false;
    if (tc.pass_to_slot < 0 || tc.pass_to_slot >= kPitchPlayers) return false;

    Entity& recv = s.players[static_cast<size_t>(tc.pass_to_slot)];
    if (PossessionSpecialState(recv)) return false;
    recv.ball_distance = PossessionBallDistSq(recv, s.ball);
    // Speed-dependent band (amiga PASSING §5). A fixed 5.7-unit radius is not
    // enough: team controls run one side per tick, so a pass at 2218 raw covers
    // ~8.7 units between two consecutive checks of this side and can step clean
    // over a fixed band without ever testing inside it.
    if (recv.ball_distance > PassArrivalBandSq(s.ball.speed)) return false;
    // Ground pass arrival — ignore lofted balls still in the air.
    if (s.ball.pos.z.Whole() > kBallZBand4) return false;

    const int outgoing = tc.controlled_slot;
    if (outgoing >= 0 && outgoing < kPitchPlayers && outgoing != tc.pass_to_slot) {
        Entity& out = s.players[static_cast<size_t>(outgoing)];
        out.dest_x = out.pos.x.Whole();
        out.dest_y = out.pos.y.Whole();
        out.delta = {};
    }

    tc.controlled_slot = tc.pass_to_slot;
    tc.pass_to_slot = -1;
    tc.player_switch_timer = kPassArrivalSwitchTicks;
    return true;
}

inline void UpdatePossession(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    TickPassKickLockout(tc);

    const int controlled = tc.controlled_slot;

    if (tc.player_has_ball) {
        if (!tc.pl_close_to_ball) {
            tc.player_has_ball = 0;
            tc.ball_out_of_play = 1;
        } else {
            tc.ball_out_of_play = 0;
        }
        return;
    }

    if (CanCapture(s, tc, controlled) && tc.pl_very_close_to_ball) {
        const bool completing_pass = tc.pass_in_progress != 0;
        // Pass completion: credit the kicker if a pass was in flight.
        if (completing_pass && tc.passing_kicking_slot >= 0 &&
            tc.passing_kicking_slot != controlled) {
            const int ksq = SquadIndexFromPitchSlot(tc.passing_kicking_slot);
            if (PlayerMatchStats* st = MatchStatsFor(s, side, ksq))
                BumpU16(st->passes_completed);
        }
        if (completing_pass) {
            tc.pass_in_progress = 0;
            // Sit at the receiver's feet until stick input (then aim-ahead).
            Entity& ball = s.ball;
            ball.speed = 0;
            ball.delta.x = Fix{};
            ball.delta.y = Fix{};
            ball.dest_x = ball.pos.x.Whole();
            ball.dest_y = ball.pos.y.Whole();
        }
        // Taking possession ends the aftertouch window. Nothing used to close
        // it, and the consequences were severe:
        //
        //  * the curl kept rewriting the dest of a ball that was now at a
        //    player's feet, so it squirted away from him ("it bounces off him");
        //  * worse, capture clears pass_in_progress, so if the ball was received
        //    before the tick-4 vertical sample the sample then took the **shot**
        //    branch and set deltaZ to the lob pair at 2688 — launching a tapped
        //    pass off the receiver's foot into the sky.
        //
        // A ball under control is not in flight, and a kick that has been
        // collected is over.
        ResetBothSpinTimers(s);

        tc.player_has_ball = 1;
        tc.ball_out_of_play = 0;
        s.clock.last_team_played = static_cast<uint8_t>(side + 1);
        if (controlled >= 0 && controlled < kPitchPlayers) {
            Entity& e = s.players[static_cast<size_t>(controlled)];
            tc.ball_controlling_player_direction = e.direction;
            e.dribble_touches = 0; // fresh possession, fresh touch budget (B13 / R4)
        }
    }
}

inline int16_t CarrierControlAttr(const MatchState& s, const Entity& player) {
    const int side_i = player.team_number - 1;
    if (side_i < 0 || side_i >= 2) return 0;
    const int ord = player.player_ordinal;
    if (ord < 1 || ord > kMatchSquadSize) return 0;
    return s.sides[static_cast<size_t>(side_i)]
        .squad[static_cast<size_t>(ord - 1)]
        .attrs.ball_control;
}

// Dribble after the carrier's speed is written for this side-tick.
// Possession is aim-ahead, not foot-glue (CONTROL §3 / §7): never teleport the
// ball onto the player. Stick release leaves dest/speed alone so the ball rolls.
// Re-aim only while pl_very_close — a 180° turn with the ball out ahead must
// risk leaving the close band.
inline void ApplyDribble(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    if (!tc.player_has_ball) return;
    if (tc.controlled_slot < 0 || tc.controlled_slot >= kPitchPlayers) return;

    const bool stick =
        tc.current_allowed_direction >= 0 && tc.current_allowed_direction <= 7;
    // Released: do not rewrite dest/speed — ball keeps rolling under UpdateBall.
    if (!stick) return;
    // Ball drifted to "close" but not "very close": cannot redirect this tick.
    if (!tc.pl_very_close_to_ball) return;

    Entity& player = s.players[static_cast<size_t>(tc.controlled_slot)];
    Entity& ball = s.ball;

    const int dir = static_cast<int>(tc.current_allowed_direction);

    // B13 / R4 — the touch count. Only a touch that *changes direction* counts:
    // running in a straight line is free, and it is turning that costs control.
    // When the count passes the carrier's Ball-Control threshold the ball gets
    // away from him — not a foul, not a tackle, just a bad touch.
    const int ctrl_idx =
        ControlAttrIndex(static_cast<uint8_t>(CarrierControlAttr(s, player)));
    const int16_t stored_dir = tc.ball_controlling_player_direction;
    if (stored_dir >= 0 && stored_dir <= 7 && static_cast<int>(stored_dir) != dir) {
        if (player.dribble_touches < 255) ++player.dribble_touches;
        if (player.dribble_touches >
            kDribbleTouchesAllowed[static_cast<size_t>(ctrl_idx)]) {
            player.dribble_touches = 0;
            tc.player_has_ball = 0;
            tc.ball_can_be_controlled = 0;
            tc.won_the_ball_timer = kTouchOverflowWonBallTicks;
            tc.ball_controlling_player_direction = static_cast<int16_t>(dir);
            return; // the ball keeps its current dest/speed and runs on
        }
    }

    const int16_t px = player.pos.x.Whole();
    const int16_t py = player.pos.y.Whole();
    const Dest nudge = kBallPlOffsets[static_cast<size_t>(dir)];
    const Dest ahead = kDefaultDestinations[static_cast<size_t>(dir)];

    // ±1 orient is a dest nudge only (not a position snap).
    ball.dest_x = static_cast<int16_t>(px + nudge.x + ahead.x);
    ball.dest_y = static_cast<int16_t>(py + nudge.y + ahead.y);
    tc.ball_controlling_player_direction = static_cast<int16_t>(dir);

    const int16_t delta = kBallSpeedDeltaWhenControlled[static_cast<size_t>(ctrl_idx)];
    // Control table = ahead offset on carrier speed (not an absolute 32…130 speed).
    if ((s.tick & 2u) != 0 || ball.speed == 0) {
        int32_t sp = static_cast<int32_t>(player.speed) + delta;
        if (sp < 0) sp = 0;
        if (sp > 32767) sp = 32767;
        ball.speed = static_cast<int16_t>(sp);
    }
}

inline void GiveBallForTest(MatchState& s, int side, int slot) {
    if (slot < 0 || slot >= kPitchPlayers) return;
    Entity& e = s.players[static_cast<size_t>(slot)];
    s.ball.pos.x = e.pos.x;
    s.ball.pos.y = e.pos.y;
    s.ball.pos.z = Fix{};
    s.ball.delta = {};
    s.ball.speed = 0;
    s.ball.dest_x = e.pos.x.Whole();
    s.ball.dest_y = e.pos.y.Whole();

    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    tc.controlled_slot = static_cast<int8_t>(slot);
    tc.player_has_ball = 1;
    tc.ball_can_be_controlled = 1;
    tc.pass_kick_timer = 0;
    tc.ball_out_of_play = 0;
    e.ball_distance = 0;
    e.dribble_touches = 0;
    ClassifyPlanarBands(tc, 0);
    ClassifyHeightBands(tc, 0);
    s.clock.last_team_played = static_cast<uint8_t>(side + 1);
}

// Bands + capture/release. Call ApplyDribble after the carrier's speed is set.
inline void UpdatePossessionForSide(MatchState& s, int side) {
    TryCompletePassArrival(s, side);
    UpdateProximityBands(s, side);
    UpdatePossession(s, side);
}

} // namespace at
