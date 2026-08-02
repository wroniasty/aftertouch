#include <SDL3/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include "core/match_engine.hpp"
#include "core/match_state.hpp"
#include "data/fictional.hpp"
#include "data/game_data.hpp"
#include "input/match_input_source.hpp"
#include "render/asset_source.hpp"
#include "render/match_renderer.hpp"
#include "tracekit/tracekit.hpp"
#include "ui_imgui/fonts.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kWindowW = 1280;
constexpr int kWindowH = 800;

constexpr int kMatchW = 320;
constexpr int kMatchH = 200;

constexpr uint64_t kTickNs = 1'000'000'000ull / at::MatchEngine::kTickHz;
constexpr uint32_t kMatchSeed = 0xC1A00001u;

// C1a visual-debug pacing (presentation only; engine tick rate unchanged).
constexpr int kSimSpeedSteps[] = {25, 50, 75, 90, 100, 110, 125, 150, 200};
constexpr int kSimSpeedStepCount =
    static_cast<int>(sizeof(kSimSpeedSteps) / sizeof(kSimSpeedSteps[0]));
constexpr int kSimSpeedDefaultIdx = 4; // 100%

enum class AppPhase { MainMenu, Match };

struct MatchRecorder {
    at::tracekit::Scenario scenario{};
    std::string            text;
    at::MatchState         prev{};
    at::MatchInput         prev_in{};
    uint8_t                prev_chron = 0;
    bool                   have_prev  = false;
    bool                   flushed    = false;

    void Reset(uint32_t seed) {
        scenario = {};
        scenario.seed = seed;
        scenario.profile = at::trace::Profile::kAmiga;
        scenario.first_team = 0;
        scenario.inputs.clear();
        text.clear();
        prev = {};
        prev_in = {};
        prev_chron = 0;
        have_prev = false;
        flushed = false;
        char hdr[160];
        std::snprintf(hdr, sizeof hdr,
                      "# MATCH transcript  seed=0x%08X  profile=amiga  hz=50\n"
                      "# live capture (ApplyKickoff bootstrap); sparse ticks\n",
                      seed);
        text = hdr;
    }

    void OnStep(const at::MatchState& st, const at::MatchInput& in) {
        scenario.inputs.push_back(in);

        bool interesting = !have_prev;
        if (have_prev) {
            if (!at::tracekit::InputsEqual(in, prev_in)) interesting = true;
            if (st.sides[0].control.controlled_slot !=
                    prev.sides[0].control.controlled_slot ||
                st.sides[1].control.controlled_slot !=
                    prev.sides[1].control.controlled_slot)
                interesting = true;
            if (st.score[0] != prev.score[0] || st.score[1] != prev.score[1])
                interesting = true;
            if (st.globals.game_state != prev.globals.game_state ||
                st.phase != prev.phase)
                interesting = true;
            if (st.chronicle.count != prev_chron) interesting = true;
            if ((st.tick % 25u) == 0) interesting = true;
        }

        if (interesting) {
            text += at::tracekit::FormatTickLine(st, in);
            if (st.chronicle.count > prev_chron) {
                for (uint8_t e = prev_chron; e < st.chronicle.count; ++e) {
                    const at::MatchEvent& ev =
                        st.chronicle.events[static_cast<size_t>(e)];
                    char eb[96];
                    std::snprintf(eb, sizeof eb,
                                  "\n  event: %s side=%u squad=%u min=%u",
                                  at::tracekit::EventKindToken(ev.kind), ev.side,
                                  ev.squad_index, ev.minute);
                    text += eb;
                }
            }
            text += '\n';
        }

        prev = st;
        prev_in = in;
        prev_chron = st.chronicle.count;
        have_prev = true;
    }

    void FlushIfNeeded() {
        if (flushed || scenario.inputs.empty()) return;
        flushed = true;

        if (!SDL_CreateDirectory(AT_TRACES_DIR)) {
            // Already exists is fine; CreateDirectory returns false if present.
        }

        char atin_path[512];
        char txt_path[512];
        std::snprintf(atin_path, sizeof atin_path, "%s/match_%08X.atin",
                      AT_TRACES_DIR, scenario.seed);
        std::snprintf(txt_path, sizeof txt_path, "%s/match_%08X.txt",
                      AT_TRACES_DIR, scenario.seed);

        std::vector<uint8_t> atin;
        if (at::tracekit::SerializeInputLog(scenario, atin) &&
            at::tracekit::WriteFile(atin_path, atin)) {
            SDL_Log("traces: wrote %s (%u ticks)", atin_path,
                    static_cast<unsigned>(scenario.inputs.size()));
        } else {
            SDL_Log("traces: failed to write %s", atin_path);
        }

        const auto span = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(text.data()), text.size());
        if (at::tracekit::WriteFile(txt_path, span)) {
            SDL_Log("traces: wrote %s (%zu bytes) — paste into agent chat",
                    txt_path, text.size());
        } else {
            SDL_Log("traces: failed to write %s", txt_path);
        }
    }
};

void SeedPlayableMatch(at::MatchEngine& engine) {
    engine.Reset(kMatchSeed);
    const at::RngStream g = engine.State().gameplay_rng;
    const at::RngStream p = engine.State().presentation_rng;
    const at::RngStream r = engine.State().resolve_rng;

    const at::data::League league = at::data::MakeFictionalLeague();
    at::MatchState sheet{};
    // Teams 1 / 2 = Northbridge FC vs Port Meridian.
    if (!at::data::ApplyKickoff(league, 1, 2, sheet)) {
        SDL_Log("ApplyKickoff failed — falling back to empty sheet");
    }

    sheet.gameplay_rng     = g;
    sheet.presentation_rng = p;
    sheet.resolve_rng      = r;
    sheet.clock.game_length = 0;
    // Leave match_started = 0 so first Step runs BeginMatchIfNeeded + kickoff place.

    sheet.sides[0].control.player_number = 1; // human home
    sheet.sides[1].control.player_number = 0; // CPU away
    // Prefer an outfield starter; B9 selection will re-pick as needed.
    sheet.sides[0].control.controlled_slot = 9;
    sheet.sides[1].control.controlled_slot = 20;

    engine.LoadState(sheet);
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
    bool                 follow_ball = true;
    int                  sim_speed_idx = kSimSpeedDefaultIdx;
    MatchRecorder        recorder;
    bool                 ft_flushed = false;

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
            if (ev.type == SDL_EVENT_QUIT) {
                if (phase == AppPhase::Match) recorder.FlushIfNeeded();
                running = false;
            }
            if (ev.type == SDL_EVENT_KEY_DOWN && phase == AppPhase::Match) {
                if (ev.key.key == SDLK_ESCAPE) {
                    recorder.FlushIfNeeded();
                    phase = AppPhase::MainMenu;
                } else if (ev.key.key == SDLK_V) {
                    follow_ball = !follow_ball;
                } else if (ev.key.key == SDLK_MINUS ||
                           ev.key.key == SDLK_KP_MINUS) {
                    if (sim_speed_idx > 0) --sim_speed_idx;
                } else if (ev.key.key == SDLK_EQUALS ||
                           ev.key.key == SDLK_KP_PLUS) {
                    if (sim_speed_idx + 1 < kSimSpeedStepCount) ++sim_speed_idx;
                } else if (ev.key.key == SDLK_0 || ev.key.key == SDLK_KP_0) {
                    sim_speed_idx = kSimSpeedDefaultIdx;
                }
            }
        }

        const uint64_t now_ns = SDL_GetTicksNS();
        uint64_t       frame_ns = now_ns - last_ns;
        last_ns = now_ns;
        if (frame_ns > 250'000'000ull) frame_ns = 250'000'000ull;

        if (phase == AppPhase::Match) {
            // Keep Stepping through HT so stoppage_event_timer can resume 2H.
            // Freeze the sim at FullTime.
            if (engine.State().phase != at::MatchPhase::FullTime) {
                const int pct = kSimSpeedSteps[sim_speed_idx];
                accumulator += frame_ns * static_cast<uint64_t>(pct) / 100ull;
                while (accumulator >= kTickNs) {
                    at::MatchInput in{};
                    input.Poll(in);
                    engine.Step(in);
                    recorder.OnStep(engine.State(), in);
                    accumulator -= kTickNs;
                    if (engine.State().phase == at::MatchPhase::FullTime) {
                        accumulator = 0;
                        break;
                    }
                }
            } else {
                accumulator = 0;
                if (!ft_flushed) {
                    recorder.FlushIfNeeded();
                    ft_flushed = true;
                }
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
            at::render::DrawMatch(renderer, engine.State(), kMatchW, kMatchH,
                                  follow_ball);
            SDL_SetRenderLogicalPresentation(
                renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
            at::render::DrawMatchHud(renderer, engine.State(), follow_ball,
                                     kSimSpeedSteps[sim_speed_idx]);
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
                recorder.Reset(kMatchSeed);
                accumulator = 0;
                follow_ball = true;
                sim_speed_idx = kSimSpeedDefaultIdx;
                ft_flushed = false;
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
