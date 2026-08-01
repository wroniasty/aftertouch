#include <SDL3/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include "core/match_engine.hpp"
#include "render/asset_source.hpp"
#include "ui_imgui/fonts.hpp"

#include <cstdint>
#include <memory>

namespace {

constexpr int kWindowW = 1280;
constexpr int kWindowH = 800;

// Match viewport in logical pixels. Sacred. See section 6.
constexpr int kMatchW = 320;
constexpr int kMatchH = 200;

constexpr uint64_t kTickNs = 1'000'000'000ull / at::MatchEngine::kTickHz;

enum class AppPhase { MainMenu, Match };

} // namespace

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("aftertouch", kWindowW, kWindowH,
                                     SDL_WINDOW_RESIZABLE,
                                     &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;   // no imgui.ini turds in the cwd
    at::ui::LoadFonts();                    // see fonts.cpp below
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    at::MatchEngine engine;
    AppPhase        phase = AppPhase::MainMenu;

    // A4: prefer imported packs; clean clones run on placeholder art.
    std::unique_ptr<at::IAssetSource> assets = at::OpenAssetSource(
        AT_ASSET_DIR "/generated", AT_ASSET_DIR "/placeholder");
    if (!assets) {
        SDL_Log("OpenAssetSource failed — missing assets/placeholder?");
        return 1;
    }
    if (assets->IsPlaceholder()) {
        SDL_Log("assets: placeholder (import with assetc into assets/generated)");
    } else {
        SDL_Log("assets: imported");
    }
    (void)assets; // C1 draws through this; match_renderer still a stub.

    uint64_t last_ns     = SDL_GetTicksNS();
    uint64_t accumulator = 0;
    bool     running     = true;

    while (running) {
        // ---- events ----------------------------------------------------
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT) running = false;
            if (ev.type == SDL_EVENT_KEY_DOWN &&
                ev.key.key == SDLK_ESCAPE && phase == AppPhase::Match) {
                phase = AppPhase::MainMenu;
            }
        }

        // ---- fixed timestep --------------------------------------------
        const uint64_t now_ns = SDL_GetTicksNS();
        uint64_t       frame_ns = now_ns - last_ns;
        last_ns = now_ns;

        // Clamp to avoid the spiral of death after a breakpoint or a stall.
        if (frame_ns > 250'000'000ull) frame_ns = 250'000'000ull;

        if (phase == AppPhase::Match) {
            accumulator += frame_ns;
            while (accumulator >= kTickNs) {
                at::MatchInput in{};      // real input mapping comes later
                engine.Step(in);
                accumulator -= kTickNs;
            }
        } else {
            accumulator = 0;
        }

        // ---- render: match pass ----------------------------------------
        SDL_SetRenderDrawColor(renderer, 12, 14, 18, 255);
        SDL_RenderClear(renderer);

        if (phase == AppPhase::Match) {
            SDL_SetRenderLogicalPresentation(
                renderer, kMatchW, kMatchH,
                SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

            SDL_SetRenderDrawColor(renderer, 20, 90, 40, 255);
            SDL_FRect pitch{0, 0, (float)kMatchW, (float)kMatchH};
            SDL_RenderFillRect(renderer, &pitch);

            SDL_SetRenderDrawColor(renderer, 235, 235, 235, 255);
            SDL_RenderDebugText(renderer, 8.0f, 8.0f, "In progress..");

            char tickbuf[64];
            SDL_snprintf(tickbuf, sizeof tickbuf, "tick %u",
                         engine.State().tick);
            SDL_RenderDebugText(renderer, 8.0f, 20.0f, tickbuf);
            SDL_RenderDebugText(renderer, 8.0f, 32.0f, "ESC to return");

            SDL_SetRenderLogicalPresentation(
                renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        }

        // ---- render: UI pass -------------------------------------------
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (phase == AppPhase::MainMenu) {
            ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
            ImGui::Begin("aftertouch");
            if (ImGui::Button("MATCH", ImVec2(280, 48))) {
                engine.Reset(1);
                accumulator = 0;
                phase = AppPhase::Match;
            }
            // Diacritic smoke test (PLAN.md milestone-1 DoD): Polish, Turkish,
            // Greek. If these render as boxes, the glyph ranges or the TTF are
            // wrong. See ui_imgui/fonts.cpp. The u8 literal guarantees UTF-8
            // bytes regardless of source encoding / compiler codepage; the cast
            // hands them to ImGui's narrow-string API unchanged.
            ImGui::TextUnformatted(reinterpret_cast<const char*>(
                u8"Zażółć gęślą jaźń  Gğüşıöç  Καλημέρα"));
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
