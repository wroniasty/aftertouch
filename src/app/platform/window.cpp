#include "platform/window.hpp"

#include <SDL3/SDL.h>

namespace at::platform {

bool CreateWindow(const char* title, int w, int h, Window& out) {
    if (!SDL_CreateWindowAndRenderer(title, w, h, SDL_WINDOW_RESIZABLE,
                                     &out.window, &out.renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(out.renderer, 1);
    return true;
}

void DestroyWindow(Window& w) {
    if (w.renderer) SDL_DestroyRenderer(w.renderer);
    if (w.window)   SDL_DestroyWindow(w.window);
    w.renderer = nullptr;
    w.window   = nullptr;
}

} // namespace at::platform
