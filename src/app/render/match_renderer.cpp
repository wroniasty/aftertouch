#include "render/match_renderer.hpp"
#include "render/pitch_view.hpp"

#include "core/match_clock.hpp"
#include "core/match_state.hpp"
#include "core/set_pieces.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace at::render {

namespace {

// Match possession.hpp kDistVeryCloseSq / kDistCloseSq — capture footprint in
// whole pitch units. Drawn radius ≈ that circle on screen.
constexpr int32_t kCaptureRadiusSq = 32; // ~5.7 u
constexpr int32_t kCloseRadiusSq   = 72; // ~8.5 u
constexpr int16_t kCentreCircleR   = 55;

void FillCircle(SDL_Renderer* r, int cx, int cy, int radius, Uint8 R, Uint8 G,
                Uint8 B) {
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    if (radius <= 0) {
        SDL_RenderPoint(r, static_cast<float>(cx), static_cast<float>(cy));
        return;
    }
    const int r2 = radius * radius;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= r2)
                SDL_RenderPoint(r, static_cast<float>(cx + dx),
                                static_cast<float>(cy + dy));
        }
    }
}

void StrokeCircle(SDL_Renderer* r, int cx, int cy, int radius, Uint8 R, Uint8 G,
                  Uint8 B) {
    if (radius <= 0) {
        FillCircle(r, cx, cy, 0, R, G, B);
        return;
    }
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    // Midpoint-ish: plot 8-way points every ~1 px along the circumference.
    const int steps = radius * 6;
    for (int i = 0; i < steps; ++i) {
        const float a =
            (static_cast<float>(i) * 6.2831853f) / static_cast<float>(steps);
        const int x = cx + static_cast<int>(std::lround(std::cos(a) * radius));
        const int y = cy + static_cast<int>(std::lround(std::sin(a) * radius));
        SDL_RenderPoint(r, static_cast<float>(x), static_cast<float>(y));
    }
}

void StrokeRing(SDL_Renderer* r, int cx, int cy, int radius, Uint8 R, Uint8 G,
                Uint8 B) {
    StrokeCircle(r, cx, cy, radius, R, G, B);
    if (radius > 1) StrokeCircle(r, cx, cy, radius - 1, R, G, B);
}

int WorldRadiusToScreen(int32_t radius_sq, float scale) {
    const float world_r = std::sqrt(static_cast<float>(radius_sq));
    const int px = static_cast<int>(std::lround(world_r * scale));
    return px < 1 ? 1 : px;
}

void DrawLinePitch(SDL_Renderer* r, const DebugView& view, int match_w,
                   int match_h, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    const auto a = PitchToScreen(x0, y0, view, match_w, match_h);
    const auto b = PitchToScreen(x1, y1, view, match_w, match_h);
    SDL_RenderLine(r, static_cast<float>(a.x), static_cast<float>(a.y),
                   static_cast<float>(b.x), static_cast<float>(b.y));
}

void DrawBoxPitch(SDL_Renderer* r, const DebugView& view, int match_w,
                  int match_h, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    DrawLinePitch(r, view, match_w, match_h, x0, y0, x1, y0);
    DrawLinePitch(r, view, match_w, match_h, x1, y0, x1, y1);
    DrawLinePitch(r, view, match_w, match_h, x1, y1, x0, y1);
    DrawLinePitch(r, view, match_w, match_h, x0, y1, x0, y0);
}

void DrawLandmarks(SDL_Renderer* r, const DebugView& view, int match_w,
                   int match_h) {
    SDL_SetRenderDrawColor(r, 40, 120, 55, 255);
    // Outer dead-ball / playable frame (clipped to view).
    DrawBoxPitch(r, view, match_w, match_h, kViewMinX, kViewMinY, kViewMaxX,
                 kViewMaxY);
    DrawBoxPitch(r, view, match_w, match_h, kPlayableMinX, kPlayableMinY,
                 kPlayableMaxX, kPlayableMaxY);

    // Halfway + centre spot.
    DrawLinePitch(r, view, match_w, match_h, kPlayableMinX, kCentreSpotY,
                  kPlayableMaxX, kCentreSpotY);
    {
        const auto c =
            PitchToScreen(kCentreSpotX, kCentreSpotY, view, match_w, match_h);
        FillCircle(r, c.x, c.y, 1, 50, 140, 65);
        const float scale = PitchUniformScale(view, match_w, match_h);
        const int cr =
            static_cast<int>(std::lround(static_cast<float>(kCentreCircleR) * scale));
        StrokeCircle(r, c.x, c.y, cr < 2 ? 2 : cr, 40, 120, 55);
    }

    // Penalty boxes (SIMULATION.md / SETPIECES.md).
    DrawBoxPitch(r, view, match_w, match_h, kPenBoxXMin, kPlayableMinY,
                 kPenBoxXMax, kPenaltyBoxTopY);
    DrawBoxPitch(r, view, match_w, match_h, kPenBoxXMin, kPenaltyBoxBotY,
                 kPenBoxXMax, kPlayableMaxY);

    // Goal mouths.
    SDL_SetRenderDrawColor(r, 55, 140, 70, 255);
    DrawLinePitch(r, view, match_w, match_h, kGoalMouthMinX, kPlayableMinY,
                  kGoalMouthMaxX, kPlayableMinY);
    DrawLinePitch(r, view, match_w, match_h, kGoalMouthMinX, kPlayableMaxY,
                  kGoalMouthMaxX, kPlayableMaxY);
}

DebugView ViewForState(const MatchState& state, bool follow_ball) {
    if (!follow_ball) return FullPitchView();
    int16_t cx = state.ball.pos.x.Whole();
    int16_t cy = state.ball.pos.y.Whole();
    // Fallback: home controlled player if ball coords look unset.
    if (cx == 0 && cy == 0) {
        const int8_t slot = state.sides[0].control.controlled_slot;
        if (slot >= 0 && slot < kPitchPlayers) {
            cx = state.players[static_cast<size_t>(slot)].pos.x.Whole();
            cy = state.players[static_cast<size_t>(slot)].pos.y.Whole();
        } else {
            cx = kCentreSpotX;
            cy = kCentreSpotY;
        }
    }
    return WindowAround(cx, cy);
}

const char* GameStateName(GameState gs) {
    switch (gs) {
    case GameState::PlayersToInitialPositions: return "SETUP";
    case GameState::GoalOutLeft:               return "GK-L";
    case GameState::GoalOutRight:              return "GK-R";
    case GameState::KeeperHoldsBall:           return "HOLD";
    case GameState::CornerLeft:                return "CR-L";
    case GameState::CornerRight:               return "CR-R";
    case GameState::FreeKickLeft1:             return "FK-L1";
    case GameState::FreeKickLeft2:             return "FK-L2";
    case GameState::FreeKickLeft3:             return "FK-L3";
    case GameState::FreeKickCentre:            return "FK-C";
    case GameState::FreeKickRight1:            return "FK-R1";
    case GameState::FreeKickRight2:            return "FK-R2";
    case GameState::FreeKickRight3:            return "FK-R3";
    case GameState::Foul:                      return "FOUL";
    case GameState::Penalty:                   return "PEN";
    case GameState::ThrowInForwardRight:       return "TI-FR";
    case GameState::ThrowInCentreRight:        return "TI-CR";
    case GameState::ThrowInBackRight:          return "TI-BR";
    case GameState::ThrowInForwardLeft:        return "TI-FL";
    case GameState::ThrowInCentreLeft:         return "TI-CL";
    case GameState::ThrowInBackLeft:           return "TI-BL";
    case GameState::StartingGame:              return "KO";
    case GameState::CameraGoingToShowers:      return "SHOWER";
    case GameState::GoingToHalfTime:           return "→HT";
    case GameState::PlayersGoingToShower:      return "SHOWER";
    case GameState::ResultOnHalfTime:          return "HT-RES";
    case GameState::ResultAfterGame:           return "FT-RES";
    case GameState::FirstExtraStarting:        return "ET";
    case GameState::FirstExtraEnded:           return "ET-END";
    case GameState::FirstHalfEnded:            return "HT";
    case GameState::GameEnded:                 return "FT";
    case GameState::Penalties:                 return "PENS";
    }
    return "?";
}

const char* PhaseLabel(MatchPhase phase, uint8_t period) {
    switch (phase) {
    case MatchPhase::KickOff:  return "KO";
    case MatchPhase::InPlay:   return period == 0 ? "1H" : "2H";
    case MatchPhase::Goal:     return "GOAL";
    case MatchPhase::HalfTime: return "HT";
    case MatchPhase::FullTime: return "FT";
    }
    return "?";
}

constexpr int kHeightPixelsPerUnit = 1;
constexpr int kMaxHeightLift = 40;

} // namespace

void DrawMatch(SDL_Renderer* r, const MatchState& state, int match_w, int match_h,
               bool follow_ball) {
    SDL_SetRenderDrawColor(r, 20, 90, 40, 255);
    SDL_FRect pitch{0, 0, static_cast<float>(match_w), static_cast<float>(match_h)};
    SDL_RenderFillRect(r, &pitch);

    const DebugView view = ViewForState(state, follow_ball);
    DrawLandmarks(r, view, match_w, match_h);

    const float scale = PitchUniformScale(view, match_w, match_h);
    const int player_r = WorldRadiusToScreen(kCaptureRadiusSq, scale);
    const int ctrl_r   = WorldRadiusToScreen(kCloseRadiusSq, scale);

    const int8_t home_pass = state.sides[0].control.pass_to_slot;
    const int8_t away_pass = state.sides[1].control.pass_to_slot;

    for (int i = 0; i < kPitchPlayers; ++i) {
        const Entity& e = state.players[static_cast<size_t>(i)];
        const auto sp =
            PitchToScreen(e.pos.x.Whole(), e.pos.y.Whole(), view, match_w, match_h);
        const bool home = e.team_number == 1;
        const int side = home ? 0 : 1;
        const bool ctrl =
            state.sides[static_cast<size_t>(side)].control.controlled_slot == i;
        const bool has_ball =
            state.sides[static_cast<size_t>(side)].control.player_has_ball &&
            state.sides[static_cast<size_t>(side)].control.controlled_slot == i;
        const bool pass_tgt = (i == home_pass) || (i == away_pass);
        const bool gk = e.player_ordinal == 1;
        const int rad = ctrl ? ctrl_r : player_r;

        Uint8 R, G, B;
        if (home) {
            R = ctrl ? 70 : (gk ? 25 : 40);
            G = ctrl ? 150 : (gk ? 90 : 120);
            B = ctrl ? 255 : (gk ? 180 : 220);
        } else {
            R = ctrl ? 255 : (gk ? 160 : 220);
            G = ctrl ? 90 : (gk ? 40 : 60);
            B = ctrl ? 90 : (gk ? 40 : 60);
        }
        FillCircle(r, sp.x, sp.y, rad, R, G, B);

        if (has_ball)
            StrokeRing(r, sp.x, sp.y, rad + 2, 245, 245, 245);
        else if (pass_tgt)
            StrokeRing(r, sp.x, sp.y, rad + 2, 200, 200, 80);
    }

    {
        const int16_t bx = state.ball.pos.x.Whole();
        const int16_t by = state.ball.pos.y.Whole();
        const int z = std::max(0, static_cast<int>(state.ball.pos.z.Whole()));
        const auto ground = PitchToScreen(bx, by, view, match_w, match_h);
        const int lift = std::min(z * kHeightPixelsPerUnit, kMaxHeightLift);
        const int ball_y = ground.y - lift;
        const int ball_r = (z >= 12) ? 1 : 0;

        FillCircle(r, ground.x, ground.y, 0, 15, 50, 25);
        if (lift > 1) {
            SDL_SetRenderDrawColor(r, 30, 70, 35, 255);
            SDL_RenderLine(r, static_cast<float>(ground.x),
                           static_cast<float>(ground.y),
                           static_cast<float>(ground.x),
                           static_cast<float>(ball_y));
        }

        const auto dest =
            PitchToScreen(state.ball.dest_x, state.ball.dest_y, view, match_w,
                          match_h);
        SDL_SetRenderDrawColor(r, 180, 180, 60, 255);
        SDL_RenderLine(r, static_cast<float>(ground.x),
                       static_cast<float>(ground.y), static_cast<float>(dest.x),
                       static_cast<float>(dest.y));
        FillCircle(r, dest.x, dest.y, 0, 200, 200, 80);

        FillCircle(r, ground.x, ball_y, ball_r, 245, 245, 80);
    }
}

void DrawMatchHud(SDL_Renderer* r, const MatchState& state, bool follow_ball,
                  int sim_speed_pct) {
    SDL_SetRenderDrawColor(r, 235, 235, 235, 255);
    char line[160];

    const int minute = static_cast<int>(state.clock.displayed_minute);
    int sec = static_cast<int>(state.clock.game_seconds);
    if (sec < 0) sec = 0;
    if (sec > 59) sec = 59;

    const GameState gs = GetGameState(state);
    const GameStatePl pl = GetPl(state);
    const char* pl_tag =
        (pl == GameStatePl::InProgress)     ? ""
        : (pl == GameStatePl::Stopped)      ? " STOP"
        : (pl == GameStatePl::WaitingOnPlayer) ? " WAIT"
                                              : "";

    const int whole = sim_speed_pct / 100;
    const int frac = sim_speed_pct % 100;
    SDL_snprintf(line, sizeof line, "%d'%02d  %u-%u  %s  %s%s  %s  sim %d.%02dx",
                 minute, sec, state.score[0], state.score[1],
                 PhaseLabel(state.phase, state.clock.period), GameStateName(gs),
                 pl_tag, follow_ball ? "cam:follow" : "cam:full", whole, frac);
    SDL_RenderDebugText(r, 8.0f, 8.0f, line);

    const auto& tc = state.sides[0].control;
    SDL_snprintf(line, sizeof line,
                 "P%d has=%d tgt=%d  z=%d spd=%d  spin=%d%s%s  fire=%d",
                 static_cast<int>(tc.controlled_slot),
                 static_cast<int>(tc.player_has_ball),
                 static_cast<int>(tc.pass_to_slot), state.ball.pos.z.Whole(),
                 state.ball.speed, static_cast<int>(tc.spin_timer),
                 tc.left_spin ? " L" : "", tc.right_spin ? " R" : "",
                 static_cast<int>(tc.fire_counter));
    SDL_RenderDebugText(r, 8.0f, 20.0f, line);

    SDL_RenderDebugText(
        r, 8.0f, 32.0f,
        "tap=pass  hold=shot  V=cam  -/+=speed  0=1x  ESC=menu");

    if (state.phase == MatchPhase::HalfTime) {
        SDL_SetRenderDrawColor(r, 255, 220, 120, 255);
        SDL_snprintf(line, sizeof line, "[HALF TIME] %u-%u  — resuming…",
                     state.score[0], state.score[1]);
        SDL_RenderDebugText(r, 8.0f, 48.0f, line);
    } else if (state.phase == MatchPhase::FullTime) {
        SDL_SetRenderDrawColor(r, 255, 200, 100, 255);
        SDL_snprintf(line, sizeof line, "[FULL TIME] %u-%u  — ESC menu",
                     state.score[0], state.score[1]);
        SDL_RenderDebugText(r, 8.0f, 48.0f, line);
    }
}

} // namespace at::render
