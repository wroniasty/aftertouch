#include "render/match_renderer.hpp"
#include "core/match_state.hpp"

#include <SDL3/SDL.h>

namespace at::render {

void DrawMatch(SDL_Renderer* r, const MatchState& state, int match_w, int match_h) {
    SDL_SetRenderDrawColor(r, 20, 90, 40, 255);
    SDL_FRect pitch{0, 0, (float)match_w, (float)match_h};
    SDL_RenderFillRect(r, &pitch);

    SDL_SetRenderDrawColor(r, 235, 235, 235, 255);
    SDL_RenderDebugText(r, 8.0f, 8.0f, "In progress..");

    char tickbuf[64];
    SDL_snprintf(tickbuf, sizeof tickbuf, "tick %u", state.tick);
    SDL_RenderDebugText(r, 8.0f, 20.0f, tickbuf);
    SDL_RenderDebugText(r, 8.0f, 32.0f, "ESC to return");
}

} // namespace at::render
