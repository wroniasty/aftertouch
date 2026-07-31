#include "ui_imgui/imgui_backend.hpp"
#include "ui_imgui/fonts.hpp"
#include "ui_imgui/screens/main_menu.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

namespace at {

bool ImGuiBackend::Init(void* window, void* renderer) {
    auto* win = static_cast<SDL_Window*>(window);
    auto* ren = static_cast<SDL_Renderer*>(renderer);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ui::LoadFonts();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForSDLRenderer(win, ren)) return false;
    if (!ImGui_ImplSDLRenderer3_Init(ren)) return false;
    return true;
}

void ImGuiBackend::Shutdown() {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

bool ImGuiBackend::HandleEvent(const void* sdl_event) {
    return ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(sdl_event));
}

void ImGuiBackend::BeginFrame() {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

Intent ImGuiBackend::DrawScreen(ScreenId screen, AppModel& model) {
    switch (screen) {
        case ScreenId::MainMenu: return ui::DrawMainMenu(model);
        case ScreenId::Match:    return Intent{};
    }
    return Intent{};
}

void ImGuiBackend::EndFrame(void* renderer) {
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(
        ImGui::GetDrawData(), static_cast<SDL_Renderer*>(renderer));
}

} // namespace at
