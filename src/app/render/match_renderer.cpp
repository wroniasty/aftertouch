#include "render/match_renderer.hpp"
#include "render/pitch_view.hpp"

#include "core/match_state.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace at::render {

namespace {

// Match possession.hpp kDistVeryCloseSq / kDistCloseSq — capture footprint in
// whole pitch units. Drawn radius ≈ that circle on screen.
constexpr int32_t kCaptureRadiusSq = 32; // ~5.7 u
constexpr int32_t kCloseRadiusSq   = 72; // ~8.5 u

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

int WorldRadiusToScreen(int32_t radius_sq, float scale) {
    const float world_r = std::sqrt(static_cast<float>(radius_sq));
    const int px = static_cast<int>(std::lround(world_r * scale));
    return px < 1 ? 1 : px;
}

constexpr int kHeightPixelsPerUnit = 1;
constexpr int kMaxHeightLift = 40;

} // namespace

void DrawMatch(SDL_Renderer* r, const MatchState& state, int match_w, int match_h) {
    SDL_SetRenderDrawColor(r, 20, 90, 40, 255);
    SDL_FRect pitch{0, 0, static_cast<float>(match_w), static_cast<float>(match_h)};
    SDL_RenderFillRect(r, &pitch);

    SDL_SetRenderDrawColor(r, 40, 120, 55, 255);
    const auto tl = PitchToScreen(kViewMinX, kViewMinY, match_w, match_h);
    const auto br = PitchToScreen(kViewMaxX, kViewMaxY, match_w, match_h);
    SDL_RenderLine(r, static_cast<float>(tl.x), static_cast<float>(tl.y),
                   static_cast<float>(br.x), static_cast<float>(tl.y));
    SDL_RenderLine(r, static_cast<float>(tl.x), static_cast<float>(br.y),
                   static_cast<float>(br.x), static_cast<float>(br.y));
    SDL_RenderLine(r, static_cast<float>(tl.x), static_cast<float>(tl.y),
                   static_cast<float>(tl.x), static_cast<float>(br.y));
    SDL_RenderLine(r, static_cast<float>(br.x), static_cast<float>(tl.y),
                   static_cast<float>(br.x), static_cast<float>(br.y));
    const auto mid = PitchToScreen(336, 449, match_w, match_h);
    SDL_RenderLine(r, static_cast<float>(tl.x), static_cast<float>(mid.y),
                   static_cast<float>(br.x), static_cast<float>(mid.y));

    const float scale = PitchUniformScale(match_w, match_h);
    const int player_r = WorldRadiusToScreen(kCaptureRadiusSq, scale);
    const int ctrl_r   = WorldRadiusToScreen(kCloseRadiusSq, scale);

    for (int i = 0; i < kPitchPlayers; ++i) {
        const Entity& e = state.players[static_cast<size_t>(i)];
        const auto sp = PitchToScreen(e.pos.x.Whole(), e.pos.y.Whole(), match_w, match_h);
        const bool home = e.team_number == 1;
        const int side = home ? 0 : 1;
        const bool ctrl =
            state.sides[static_cast<size_t>(side)].control.controlled_slot == i;
        const int rad = ctrl ? ctrl_r : player_r;
        if (home)
            FillCircle(r, sp.x, sp.y, rad, ctrl ? 70 : 40, ctrl ? 150 : 120,
                       ctrl ? 255 : 220);
        else
            FillCircle(r, sp.x, sp.y, rad, ctrl ? 255 : 220, ctrl ? 90 : 60,
                       ctrl ? 90 : 60);
    }

    {
        const int16_t bx = state.ball.pos.x.Whole();
        const int16_t by = state.ball.pos.y.Whole();
        const int z = std::max(0, static_cast<int>(state.ball.pos.z.Whole()));
        const auto ground = PitchToScreen(bx, by, match_w, match_h);
        const int lift = std::min(z * kHeightPixelsPerUnit, kMaxHeightLift);
        const int ball_y = ground.y - lift;
        // Ball is much smaller than the capture footprint (~5.7 u → ~2 px).
        const int ball_r = (z >= 12) ? 1 : 0;

        FillCircle(r, ground.x, ground.y, 0, 15, 50, 25);
        if (lift > 1) {
            SDL_SetRenderDrawColor(r, 30, 70, 35, 255);
            SDL_RenderLine(r, static_cast<float>(ground.x), static_cast<float>(ground.y),
                           static_cast<float>(ground.x), static_cast<float>(ball_y));
        }

        const auto dest =
            PitchToScreen(state.ball.dest_x, state.ball.dest_y, match_w, match_h);
        SDL_SetRenderDrawColor(r, 180, 180, 60, 255);
        SDL_RenderLine(r, static_cast<float>(ground.x), static_cast<float>(ground.y),
                       static_cast<float>(dest.x), static_cast<float>(dest.y));
        FillCircle(r, dest.x, dest.y, 0, 200, 200, 80);

        FillCircle(r, ground.x, ball_y, ball_r, 245, 245, 80);
    }
}

void DrawMatchHud(SDL_Renderer* r, const MatchState& state) {
    // Window pixels, native 8×8 debug glyphs — ~4× smaller than inside 320×200
    // integer scale (was ~32px on a 4× window), still readable.
    SDL_SetRenderDrawColor(r, 235, 235, 235, 255);
    char line[96];
    SDL_snprintf(line, sizeof line, "tick %u  %u-%u", state.tick, state.score[0],
                 state.score[1]);
    SDL_RenderDebugText(r, 8.0f, 8.0f, line);

    const auto& tc = state.sides[0].control;
    SDL_snprintf(line, sizeof line, "ball z=%d spd=%d  spin=%d%s%s  has=%d fire=%d",
                 state.ball.pos.z.Whole(), state.ball.speed,
                 static_cast<int>(tc.spin_timer),
                 tc.left_spin ? " L" : "", tc.right_spin ? " R" : "",
                 static_cast<int>(tc.player_has_ball),
                 static_cast<int>(tc.fire_counter));
    SDL_RenderDebugText(r, 8.0f, 20.0f, line);
    SDL_RenderDebugText(r, 8.0f, 32.0f,
                        "tap=pass  hold~0.25s=shot  aftertouch: steer after kick");
}

} // namespace at::render
