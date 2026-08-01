#include <SDL3/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include "core/match_engine.hpp"
#include "core/match_state.hpp"
#include "core/movement.hpp"
#include "input/match_input_source.hpp"
#include "render/asset_source.hpp"
#include "render/match_renderer.hpp"
#include "ui_imgui/fonts.hpp"

#include <cstdint>
#include <memory>

namespace {

constexpr int kWindowW = 1280;
constexpr int kWindowH = 800;

constexpr int kMatchW = 320;
constexpr int kMatchH = 200;

constexpr uint64_t kTickNs = 1'000'000'000ull / at::MatchEngine::kTickHz;

enum class AppPhase { MainMenu, Match };

void SeedPlayableMatch(at::MatchEngine& engine) {
    engine.Reset(0xC1A00001u);
    // First Step places players; we then overlay human/CPU + tactics.
    engine.Step(at::MatchInput{});
    at::MatchState s = engine.State();
    s.sides[0].control.player_number = 1; // human home
    s.sides[1].control.player_number = 0; // CPU away
    s.sides[0].control.controlled_slot = 9;
    s.sides[1].control.controlled_slot = 20;
    for (int side = 0; side < 2; ++side) {
        for (int r = 0; r < at::kMatchTacticRoles; ++r) {
            for (int q = 0; q < at::kMatchBallQuadrants; ++q) {
                const uint8_t x = static_cast<uint8_t>((r + q) % 15);
                const uint8_t y = static_cast<uint8_t>((r * 2 + q / 5) % 16);
                s.sides[static_cast<size_t>(side)]
                    .tactics.cells[static_cast<size_t>(r)][static_cast<size_t>(q)] =
                    static_cast<uint8_t>((x << 4) | y);
            }
        }
        for (int i = 0; i < 11; ++i) {
            s.sides[static_cast<size_t>(side)].squad[static_cast<size_t>(i)].attrs.speed =
                static_cast<uint8_t>(4 + (i % 4));
        }
    }
    at::PlacePlayersAtKickoff(s);
    at::SetPl(s, at::GameStatePl::InProgress);
    at::SetGameState(s, at::GameState::StartingGame);
    s.clock.stoppage_event_timer = 0;
    s.phase = at::MatchPhase::InPlay;
    engine.LoadState(s);
}

} // namespace

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
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
    ImGui::GetIO().IniFilename = nullptr;
    at::ui::LoadFonts();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    at::MatchEngine      engine;
    at::MatchInputSource input;
    input.Init();
    AppPhase             phase = AppPhase::MainMenu;

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
    (void)assets;

    uint64_t last_ns     = SDL_GetTicksNS();
    uint64_t accumulator = 0;
    bool     running     = true;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT) running = false;
            if (ev.type == SDL_EVENT_KEY_DOWN &&
                ev.key.key == SDLK_ESCAPE && phase == AppPhase::Match) {
                phase = AppPhase::MainMenu;
            }
        }

        const uint64_t now_ns = SDL_GetTicksNS();
        uint64_t       frame_ns = now_ns - last_ns;
        last_ns = now_ns;
        if (frame_ns > 250'000'000ull) frame_ns = 250'000'000ull;

        if (phase == AppPhase::Match) {
            accumulator += frame_ns;
            while (accumulator >= kTickNs) {
                at::MatchInput in{};
                input.Poll(in);
                engine.Step(in);
                accumulator -= kTickNs;
            }
        } else {
            accumulator = 0;
        }

        SDL_SetRenderDrawColor(renderer, 12, 14, 18, 255);
        SDL_RenderClear(renderer);

        if (phase == AppPhase::Match) {
            SDL_SetRenderLogicalPresentation(
                renderer, kMatchW, kMatchH,
                SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
            at::render::DrawMatch(renderer, engine.State(), kMatchW, kMatchH);
            SDL_SetRenderLogicalPresentation(
                renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (phase == AppPhase::MainMenu) {
            ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
            ImGui::Begin("aftertouch");
            if (ImGui::Button("MATCH", ImVec2(280, 48))) {
                SeedPlayableMatch(engine);
                accumulator = 0;
                phase = AppPhase::Match;
            }
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
