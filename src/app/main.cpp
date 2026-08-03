#include <SDL3/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include "core/match_engine.hpp"
#include "core/match_state.hpp"
#include "data/fictional.hpp"
#include "data/game_data.hpp"
#include "input/match_input_source.hpp"
#include "mode/sandbox.hpp"
#include "render/asset_source.hpp"
#include "render/camera.hpp"
#include "render/kit_palette.hpp"
#include "render/match_renderer.hpp"
#include "render/pitch_atlas.hpp"
#include "tracekit/tracekit.hpp"
#include "ui_imgui/fonts.hpp"
#include "ui_imgui/screens/sandbox_menu.hpp"

#include <cstdint>
#include <cstdio>
#include <functional>
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

// Last ~10 s of engine ticks kept for HOME clip dumps (50 Hz).
constexpr size_t kClipWindowTicks = 500;

// C1a visual-debug pacing (presentation only; engine tick rate unchanged).
constexpr int kSimSpeedSteps[] = {25, 50, 75, 90, 100, 110, 125, 150, 200};
constexpr int kSimSpeedStepCount =
    static_cast<int>(sizeof(kSimSpeedSteps) / sizeof(kSimSpeedSteps[0]));
constexpr int kSimSpeedDefaultIdx = 4; // 100%

enum class AppPhase { MainMenu, SandboxSetup, Match };

void EnsureTracesDir() {
    if (!SDL_CreateDirectory(AT_TRACES_DIR)) {
        // Already exists is fine; CreateDirectory returns false if present.
    }
}

struct MatchRecorder {
    at::tracekit::Scenario scenario{};
    std::string            text;
    at::MatchState         prev{};
    at::MatchInput         prev_in{};
    uint8_t                prev_chron = 0;
    bool                   have_prev  = false;
    bool                   flushed    = false;
    uint32_t               clip_seq   = 0;

    // Kick telemetry (B6a §4). This recorder never had a probe, so every live
    // MATCH / SANDBOX transcript — the ones actually played and reported — was
    // missing the one line that says whether a strike was a tap or a hold and
    // where it was aimed. tracegen's transcript had it; this one did not.
    at::tracekit::KickProbe kick_probe;
    std::array<int32_t, 2>  kick_reported{{-1, -1}};
    std::array<int32_t, 2>  curl_reported{{-1, -1}};

    // Ring of recent ticks for HOME window dumps.
    std::vector<at::MatchState> clip_states;
    std::vector<at::MatchInput> clip_inputs;
    size_t                      clip_head  = 0;
    size_t                      clip_count = 0;

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
        clip_seq = 0;
        kick_probe.Reset();
        kick_reported = {{-1, -1}};
        curl_reported = {{-1, -1}};
        clip_states.assign(kClipWindowTicks, {});
        clip_inputs.assign(kClipWindowTicks, {});
        clip_head = 0;
        clip_count = 0;
        char hdr[160];
        std::snprintf(hdr, sizeof hdr,
                      "# MATCH transcript  seed=0x%08X  profile=amiga  hz=50\n"
                      "# live capture (ApplyKickoff bootstrap); sparse ticks\n",
                      seed);
        text = hdr;
    }

    void PushClip(const at::MatchState& st, const at::MatchInput& in) {
        if (clip_states.empty()) {
            clip_states.assign(kClipWindowTicks, {});
            clip_inputs.assign(kClipWindowTicks, {});
        }
        if (clip_count < kClipWindowTicks) {
            clip_states[clip_count] = st;
            clip_inputs[clip_count] = in;
            ++clip_count;
            return;
        }
        clip_states[clip_head] = st;
        clip_inputs[clip_head] = in;
        clip_head = (clip_head + 1) % kClipWindowTicks;
    }

    void OnStep(const at::MatchState& st, const at::MatchInput& in) {
        scenario.inputs.push_back(in);
        PushClip(st, in);

        kick_probe.Observe(st, in);
        bool struck = false;
        bool curled = false;
        for (int side = 0; side < 2; ++side) {
            const at::tracekit::KickTelemetry* k = kick_probe.Last(side);
            if (!k) continue;
            const size_t i = static_cast<size_t>(side);
            if (k->strike_tick != kick_reported[i]) {
                kick_reported[i] = k->strike_tick;
                struck = true;
            }
            if (k->window_closed && k->strike_tick != curl_reported[i]) {
                curl_reported[i] = k->strike_tick;
                curled = true;
            }
        }

        // A strike is always worth a line, whatever else the tick did.
        bool interesting = !have_prev || struck || curled;
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
            for (int side = 0; side < 2; ++side) {
                const at::tracekit::KickTelemetry* k = kick_probe.Last(side);
                if (!k) continue;
                const size_t i = static_cast<size_t>(side);
                if (struck && k->strike_tick == static_cast<int32_t>(st.tick))
                    text += at::tracekit::FormatKickLine(side, *k);
                if (curled && k->window_closed && k->strike_tick == curl_reported[i])
                    text += at::tracekit::FormatCurlLine(side, *k);
            }
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

    // HOME: dump last ≤10 s as dense tick lines → match_<seed>_<nn>.txt
    bool SaveClipWindow() {
        if (clip_count == 0) {
            SDL_Log("traces: HOME clip empty (no ticks yet)");
            return false;
        }

        EnsureTracesDir();
        ++clip_seq;

        const size_t n = clip_count;
        const size_t first_i =
            (clip_count < kClipWindowTicks) ? 0 : clip_head;
        const at::MatchState& first = clip_states[first_i];
        const size_t last_i =
            (clip_count < kClipWindowTicks)
                ? (clip_count - 1)
                : ((clip_head + kClipWindowTicks - 1) % kClipWindowTicks);
        const at::MatchState& last = clip_states[last_i];

        char path[512];
        std::snprintf(path, sizeof path, "%s/match_%08X_%02u.txt", AT_TRACES_DIR,
                      scenario.seed, clip_seq);

        std::string out;
        char hdr[256];
        const double secs =
            static_cast<double>(n) / static_cast<double>(at::MatchEngine::kTickHz);
        std::snprintf(hdr, sizeof hdr,
                      "# MATCH clip  seed=0x%08X  clip=%02u  ticks=%u..%u  "
                      "count=%zu  window=%.2fs @ %uHz\n"
                      "# dense (every tick); HOME captures last ~10s\n"
                      "# Hpos/Apos: slot@(x,y)  *=controlled  B=has ball\n",
                      scenario.seed, clip_seq, first.tick, last.tick, n, secs,
                      at::MatchEngine::kTickHz);
        out = hdr;

        uint8_t chron = first.chronicle.count;
        for (size_t i = 0; i < n; ++i) {
            const size_t idx =
                (clip_count < kClipWindowTicks)
                    ? i
                    : (clip_head + i) % kClipWindowTicks;
            const at::MatchState& st = clip_states[idx];
            const at::MatchInput& in = clip_inputs[idx];
            out += at::tracekit::FormatTickLine(st, in);
            if (st.chronicle.count > chron) {
                for (uint8_t e = chron; e < st.chronicle.count; ++e) {
                    const at::MatchEvent& ev =
                        st.chronicle.events[static_cast<size_t>(e)];
                    char eb[96];
                    std::snprintf(eb, sizeof eb,
                                  "\n  event: %s side=%u squad=%u min=%u",
                                  at::tracekit::EventKindToken(ev.kind), ev.side,
                                  ev.squad_index, ev.minute);
                    out += eb;
                }
            }
            chron = st.chronicle.count;
            out += '\n';
        }

        const auto span = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(out.data()), out.size());
        if (at::tracekit::WriteFile(path, span)) {
            SDL_Log("traces: wrote %s (%zu ticks, %zu bytes)", path, n,
                    out.size());
            return true;
        }
        SDL_Log("traces: failed to write %s", path);
        return false;
    }

    void FlushIfNeeded() {
        if (flushed || scenario.inputs.empty()) return;
        flushed = true;

        EnsureTracesDir();

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

void SeedPlayableMatch(at::MatchEngine& engine, uint32_t seed) {
    engine.Reset(seed);
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

// Dev affordances, not shipping features: jump straight into a match and, optionally,
// save a frame and quit. "Does it look right" is C2/C3's acceptance, and a screenshot
// that anyone can regenerate on demand beats describing it.
struct LaunchOptions {
    bool        straight_to_match   = false;
    bool        straight_to_sandbox = false;
    const char* shot_path           = nullptr;
    int         shot_after_frames   = 120;
};

LaunchOptions ParseLaunch(int argc, char** argv) {
    LaunchOptions o;
    for (int i = 1; i < argc; ++i) {
        if (SDL_strcmp(argv[i], "--match") == 0) {
            o.straight_to_match = true;
        } else if (SDL_strcmp(argv[i], "--sandbox") == 0) {
            o.straight_to_sandbox = true;
        } else if (SDL_strcmp(argv[i], "--shot") == 0 && i + 1 < argc) {
            o.shot_path = argv[++i];
        } else if (SDL_strcmp(argv[i], "--shot-after") == 0 && i + 1 < argc) {
            o.shot_after_frames = SDL_atoi(argv[++i]);
        }
    }
    return o;
}

int main(int argc, char** argv) {
    const LaunchOptions launch = ParseLaunch(argc, argv);
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
    bool                 show_debug_hud = false;
    int                  sim_speed_idx = kSimSpeedDefaultIdx;
    MatchRecorder        recorder;
    bool                 ft_flushed = false;

    // C1b: a mode is "how to build its kickoff from a seed". R rebuilds through
    // this, so the hotkey and the recorder do not care which mode is running.
    at::mode::SandboxConfig sandbox_cfg = at::mode::DefaultSandboxConfig();
    std::function<void(uint32_t)> rebuild_match;
    uint32_t active_seed = kMatchSeed;
    bool     sandbox_active = false;

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
    at::render::PitchAtlas pitch_atlas;
    at::render::Camera     camera;
    at::render::KitBank    kits;

    uint64_t last_ns     = SDL_GetTicksNS();
    uint64_t accumulator = 0;
    bool     running     = true;
    // B6a: derived control telemetry for the HUD (press->strike, latch, loft).
    at::tracekit::KickProbe kick_probe;

    // Start (or restart, on R) whichever mode rebuild_match holds.
    const auto start_mode = [&](uint32_t seed) {
        active_seed = seed;
        rebuild_match(seed);
        recorder.Reset(seed);
        accumulator = 0;
        ft_flushed = false;
        phase = AppPhase::Match;
        // Kits bake once per match, not per frame (C3 §3.2), and the camera takes its
        // one coin flip for which end to start at (CAMERA.md §8).
        kits.Build(assets->Palette(), assets->KitColourOrdinals(),
                   engine.State().sides[0].sheet, engine.State().sides[1].sheet);
        camera.Reset(engine.DrawPresentationRng());
    };

    if (launch.straight_to_sandbox) {
        sandbox_active = true;
        rebuild_match = [&engine, &sandbox_cfg](uint32_t seed) {
            at::mode::SandboxConfig c = sandbox_cfg;
            c.seed = seed;
            at::mode::StartSandbox(engine, c);
        };
        start_mode(sandbox_cfg.seed);
    } else if (launch.straight_to_match || launch.shot_path) {
        sandbox_active = false;
        rebuild_match = [&engine](uint32_t seed) { SeedPlayableMatch(engine, seed); };
        start_mode(kMatchSeed);
    }
    int frames_drawn = 0;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT) {
                if (phase == AppPhase::Match) recorder.FlushIfNeeded();
                running = false;
            }
            // A dialog with the keyboard owns the keys — hotkeys included.
            const bool ui_keys = ImGui::GetIO().WantCaptureKeyboard;
            if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && !ui_keys &&
                phase == AppPhase::Match) {
                if (ev.key.key == SDLK_ESCAPE) {
                    recorder.FlushIfNeeded();
                    phase = AppPhase::MainMenu;
                } else if (ev.key.key == SDLK_R) {
                    // Same seed = the identical kickoff; Shift+R re-rolls it.
                    recorder.FlushIfNeeded();
                    const bool reroll = (ev.key.mod & SDL_KMOD_SHIFT) != 0;
                    start_mode(reroll ? active_seed + 1u : active_seed);
                } else if (ev.key.key == SDLK_F1) {
                    show_debug_hud = !show_debug_hud;
                } else if (ev.key.key == SDLK_HOME) {
                    recorder.SaveClipWindow();
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

        // Sandbox half time swaps ends, which would flip the chosen direction.
        if (phase == AppPhase::Match && sandbox_active &&
            sandbox_cfg.reset_at_half_time &&
            engine.State().phase == at::MatchPhase::HalfTime) {
            recorder.FlushIfNeeded();
            start_mode(active_seed);
        }

        if (phase == AppPhase::Match) {
            // Keep Stepping through HT so stoppage_event_timer can resume 2H.
            // Freeze the sim at FullTime.
            if (engine.State().phase != at::MatchPhase::FullTime) {
                const int pct = kSimSpeedSteps[sim_speed_idx];
                const bool ui_keys = ImGui::GetIO().WantCaptureKeyboard;
                accumulator += frame_ns * static_cast<uint64_t>(pct) / 100ull;
                while (accumulator >= kTickNs) {
                    at::MatchInput in{};
                    // Still one input per tick when a dialog has the keyboard,
                    // so the transcript stays replayable.
                    if (ui_keys) input.PollNeutral(in);
                    else input.Poll(in);
                    engine.Step(in);
                    camera.Update(engine.State());
                    kick_probe.Observe(engine.State(), in);
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
            at::render::DrawMatch(renderer, engine.State(), kMatchW, kMatchH, &camera,
                                  assets.get(), &pitch_atlas, &kits);
            SDL_SetRenderLogicalPresentation(
                renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
            at::render::DrawMatchStatus(renderer, engine.State(),
                                        sandbox_active ? "SBX  " : nullptr);
            if (show_debug_hud)
                at::render::DrawMatchHud(renderer, engine.State(), &camera,
                                         kSimSpeedSteps[sim_speed_idx],
                                         sandbox_active ? "SBX  " : nullptr,
                                         kick_probe.Last(0));
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (phase == AppPhase::MainMenu) {
            ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(320, 240), ImGuiCond_FirstUseEver);
            ImGui::Begin("aftertouch");
            if (ImGui::Button("MATCH", ImVec2(280, 48))) {
                sandbox_active = false;
                rebuild_match = [&engine](uint32_t seed) {
                    SeedPlayableMatch(engine, seed);
                };
                show_debug_hud = false;
                sim_speed_idx = kSimSpeedDefaultIdx;
                start_mode(kMatchSeed);
            }
            if (ImGui::Button("SANDBOX", ImVec2(280, 48)))
                phase = AppPhase::SandboxSetup;
            ImGui::TextUnformatted(reinterpret_cast<const char*>(
                u8"Zażółć gęślą jaźń  Gğüşıöç  Καλημέρα"));
            ImGui::End();
        } else if (phase == AppPhase::SandboxSetup) {
            const at::ui::SandboxAction action =
                at::ui::DrawSandboxMenu(sandbox_cfg);
            if (action == at::ui::SandboxAction::Back) {
                phase = AppPhase::MainMenu;
            } else if (action == at::ui::SandboxAction::Start) {
                sandbox_active = true;
                rebuild_match = [&engine, &sandbox_cfg](uint32_t seed) {
                    at::mode::SandboxConfig c = sandbox_cfg;
                    c.seed = seed;
                    at::mode::StartSandbox(engine, c);
                };
                show_debug_hud = false;
                sim_speed_idx = kSimSpeedDefaultIdx;
                start_mode(sandbox_cfg.seed);
            }
        }

        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

        ++frames_drawn;
        if (launch.shot_path && frames_drawn >= launch.shot_after_frames) {
            if (SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr)) {
                if (!SDL_SaveBMP(shot, launch.shot_path))
                    SDL_Log("screenshot failed: %s", SDL_GetError());
                else
                    SDL_Log("wrote %s", launch.shot_path);
                SDL_DestroySurface(shot);
            }
            recorder.FlushIfNeeded();
            running = false;
        }

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
