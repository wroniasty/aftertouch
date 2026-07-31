#pragma once

struct SDL_Window;
struct SDL_Renderer;

namespace at::platform {

// A window and its renderer, created together. main.cpp owns the loop; this is
// the SDL setup it delegates to. Kept in platform/ so SDL stays confined to
// platform/, render/ and ui_imgui/ (never core, never ui/).
struct Window {
    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
};

bool CreateWindow(const char* title, int w, int h, Window& out);
void DestroyWindow(Window& w);

} // namespace at::platform
