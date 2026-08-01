#include "tracekit/hil2.hpp"
#include "tracekit/tracekit.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Two ATTR traces overlaid, scrub, jump-to-divergence (A3 §2.5).
// Optional --hil2 for a HIL2 smoke-test load.

namespace {

struct LoadedTrace {
    std::string              path;
    std::vector<uint8_t>     bytes;
    at::trace::Header        header{};
    bool                     ok = false;
};

bool LoadAttr(const char* path, LoadedTrace& out) {
    out.path = path;
    out.ok   = false;
    if (!at::tracekit::ReadFile(path, out.bytes)) return false;
    if (!at::trace::DeserializeHeader(out.bytes, out.header)) return false;
    const size_t need = at::trace::kHeaderSize +
                        size_t(out.header.record_count) * at::trace::kRecordSize;
    if (out.bytes.size() < need) return false;
    out.ok = true;
    return true;
}

bool DecodeTick(const LoadedTrace& t, uint32_t index, at::MatchState& s, at::MatchInput& in) {
    if (!t.ok || index >= t.header.record_count) return false;
    const size_t off =
        at::trace::kHeaderSize + size_t(index) * at::trace::kRecordSize;
    return at::trace::DeserializeRecord(
        std::span<const uint8_t>(t.bytes).subspan(off, at::trace::kRecordSize), s, in);
}

void DrawPitch(ImDrawList* dl, ImVec2 origin, float scale, const at::MatchState& a,
               const at::MatchState& b, bool show_b) {
    const float w = 320.f * scale;
    const float h = 200.f * scale;
    dl->AddRectFilled(origin, ImVec2(origin.x + w, origin.y + h), IM_COL32(30, 90, 40, 255));
    dl->AddRect(origin, ImVec2(origin.x + w, origin.y + h), IM_COL32(220, 220, 220, 180));

    auto plot = [&](const at::Entity& e, ImU32 col, float r) {
        const float x = origin.x + float(e.pos.x.Whole()) * scale;
        const float y = origin.y + float(e.pos.y.Whole()) * scale;
        dl->AddCircleFilled(ImVec2(x, y), r, col);
    };

    plot(a.ball, IM_COL32(255, 255, 255, 255), 4.f * scale);
    for (const auto& p : a.players) plot(p, IM_COL32(80, 140, 255, 220), 3.f * scale);
    if (show_b) {
        plot(b.ball, IM_COL32(255, 80, 80, 200), 4.f * scale);
        for (const auto& p : b.players) plot(p, IM_COL32(255, 160, 80, 160), 3.f * scale);
    }
}

} // namespace

int main(int argc, char** argv) {
    const char* path_a = nullptr;
    const char* path_b = nullptr;
    const char* hil2_path = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--hil2") == 0 && i + 1 < argc) {
            hil2_path = argv[++i];
        } else if (!path_a) {
            path_a = argv[i];
        } else if (!path_b) {
            path_b = argv[i];
        } else {
            std::fprintf(stderr, "usage: %s <engine.attr> <reference.attr> [--hil2 file.hil]\n",
                         argv[0]);
            return 2;
        }
    }
    if (!path_a || !path_b) {
        std::fprintf(stderr, "usage: %s <engine.attr> <reference.attr> [--hil2 file.hil]\n",
                     argv[0]);
        return 2;
    }

    LoadedTrace ta, tb;
    if (!LoadAttr(path_a, ta)) {
        std::fprintf(stderr, "trace_viewer: cannot load %s\n", path_a);
        return 1;
    }
    if (!LoadAttr(path_b, tb)) {
        std::fprintf(stderr, "trace_viewer: cannot load %s\n", path_b);
        return 1;
    }

    const auto diff = at::tracekit::Diff(ta.bytes, tb.bytes);
    int scrub = 0;
    if (!diff.identical) {
        // Jump scrubber to the first divergent record index when possible.
        for (uint32_t i = 0; i < ta.header.record_count; ++i) {
            at::MatchState s;
            at::MatchInput in;
            if (DecodeTick(ta, i, s, in) && s.tick == diff.tick) {
                scrub = int(i);
                break;
            }
        }
    }

    at::hil2::File hil{};
    bool hil_ok = false;
    if (hil2_path) {
        std::vector<uint8_t> hb;
        if (at::tracekit::ReadFile(hil2_path, hb) && at::hil2::Parse(hb, hil)) hil_ok = true;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("aftertouch trace_viewer", 1280, 800,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        std::fprintf(stderr, "window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT) running = false;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Trace viewer", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse);

        ImGui::Text("A: %s  (%u ticks, seed 0x%08X)", ta.path.c_str(), ta.header.record_count,
                    ta.header.seed);
        ImGui::Text("B: %s  (%u ticks, seed 0x%08X)", tb.path.c_str(), tb.header.record_count,
                    tb.header.seed);

        if (diff.identical) {
            ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "identical");
        } else {
            ImGui::TextColored(ImVec4(1.f, 0.45f, 0.35f, 1.f),
                               "first divergence tick %u  class=%s  (%s)", diff.tick,
                               at::tracekit::FieldClassName(diff.first_class),
                               diff.reason ? diff.reason : "?");
            if (ImGui::Button("Jump to divergence")) {
                for (uint32_t i = 0; i < ta.header.record_count; ++i) {
                    at::MatchState s;
                    at::MatchInput in;
                    if (DecodeTick(ta, i, s, in) && s.tick == diff.tick) {
                        scrub = int(i);
                        break;
                    }
                }
            }
            if (!diff.drift.empty()) {
                const auto& last = diff.drift.back();
                ImGui::Text("drift: %zu ticks, last L1=%llu", diff.drift.size(),
                            (unsigned long long)last.l1_position);
            }
        }

        const int max_i = int(ta.header.record_count) - 1;
        if (max_i >= 0) ImGui::SliderInt("tick index", &scrub, 0, max_i);

        at::MatchState sa{}, sb{};
        at::MatchInput ia{}, ib{};
        const bool ok_a = DecodeTick(ta, uint32_t(scrub), sa, ia);
        const bool ok_b = DecodeTick(tb, uint32_t(scrub), sb, ib);

        if (ok_a) {
            ImGui::Text("tick=%u phase=%u score=%u-%u  ball=(%d,%d,%d)", sa.tick,
                        unsigned(sa.phase), sa.score[0], sa.score[1], sa.ball.pos.x.Whole(),
                        sa.ball.pos.y.Whole(), sa.ball.pos.z.Whole());
        }
        if (ok_a && ok_b) {
            const int dx = sa.ball.pos.x.Raw() - sb.ball.pos.x.Raw();
            const int dy = sa.ball.pos.y.Raw() - sb.ball.pos.y.Raw();
            ImGui::Text("ball raw delta vs B: dx=%d dy=%d", dx, dy);
        }

        ImGui::Separator();
        ImGui::Text("Blue/white = A (engine)   Orange/red = B (reference)");
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        const float scale = 2.f;
        if (ok_a) DrawPitch(dl, origin, scale, sa, sb, ok_b);
        ImGui::Dummy(ImVec2(320.f * scale, 200.f * scale));

        if (hil_ok) {
            ImGui::Separator();
            ImGui::Text("HIL2: scenes=%u frames=%zu name=%s", hil.header.scene_count,
                        hil.frames.size(), hil.header.game_name);
        }

        ImGui::End();

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
        SDL_RenderClear(renderer);
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
