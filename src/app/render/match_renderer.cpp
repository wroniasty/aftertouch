#include "render/match_renderer.hpp"
#include "render/pitch_view.hpp"

#include "core/match_state.hpp"

#include <SDL3/SDL.h>

namespace at::render {

namespace {

void FillDot(SDL_Renderer* r, int cx, int cy, int half, Uint8 R, Uint8 G, Uint8 B) {
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_FRect rect{static_cast<float>(cx - half), static_cast<float>(cy - half),
                   static_cast<float>(half * 2 + 1), static_cast<float>(half * 2 + 1)};
    SDL_RenderFillRect(r, &rect);
}

} // namespace

void DrawMatch(SDL_Renderer* r, const MatchState& state, int match_w, int match_h) {
    // Pitch background
    SDL_SetRenderDrawColor(r, 20, 90, 40, 255);
    SDL_FRect pitch{0, 0, static_cast<float>(match_w), static_cast<float>(match_h)};
    SDL_RenderFillRect(r, &pitch);

    // Simple halfway + touchline guides in pitch space.
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

    // Players
    for (int i = 0; i < kPitchPlayers; ++i) {
        const Entity& e = state.players[static_cast<size_t>(i)];
        const auto sp = PitchToScreen(e.pos.x.Whole(), e.pos.y.Whole(), match_w, match_h);
        const bool home = e.team_number == 1;
        const int side = home ? 0 : 1;
        const bool ctrl =
            state.sides[static_cast<size_t>(side)].control.controlled_slot == i;
        if (home)
            FillDot(r, sp.x, sp.y, ctrl ? 2 : 1, 40, 120, 220);
        else
            FillDot(r, sp.x, sp.y, ctrl ? 2 : 1, 220, 60, 60);
    }

    // Ball on top
    {
        const auto sp = PitchToScreen(state.ball.pos.x.Whole(), state.ball.pos.y.Whole(),
                                      match_w, match_h);
        FillDot(r, sp.x, sp.y, 2, 245, 245, 80);
    }

    SDL_SetRenderDrawColor(r, 235, 235, 235, 255);
    char line[80];
    SDL_snprintf(line, sizeof line, "tick %u  %u-%u", state.tick, state.score[0],
                 state.score[1]);
    SDL_RenderDebugText(r, 8.0f, 8.0f, line);
    SDL_RenderDebugText(r, 8.0f, 20.0f, "arrows/WASD move  Space fire  ESC menu");
}

} // namespace at::render
