#include "preview_state.hpp"

#include "core/animation_tables.hpp"
#include "render/asset_source.hpp"
#include "render/indexed_sprite.hpp"
#include "render/kit_palette.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// C3 — the player sprite viewer.
//
// One player, three magnifications, driven by the *real* animation stepper over
// the *real* frame tables. It is not a slideshow of the bank: if the game would
// show a standing player, so does this, which is what makes it usable as
// evidence rather than decoration.
//
// It exists because the 101-frame bank is only partly classified. C3 measured
// the walk cycles, the standing poses and the prone set off the art; frames
// 30..53 and 64..100 (header, throw-in, celebration, booked, injured) are still
// unread, and PlayerState already declares StaticHeader / JumpHeader / Happy /
// Sad with no table behind them. So the tool has two modes: Table mode previews
// what ships, Raw mode scrubs the bank frame by frame so the missing tables can
// be measured and written down once.
//
// Controls: arrows / WASD / numpad steer (diagonals included), Space alternates
// header and slide tackle, H alternates happy and sad, [ and ] step frames in
// raw mode, Space-bar-less playback controls live in the panel.

namespace {

using at::spriteview::Mode;
using at::spriteview::Preview;

constexpr int   kPanelW      = 430;
constexpr int   kWindowW     = 1280;
constexpr int   kWindowH     = 820;
constexpr float kDefaultMags[3] = {1.0f, 4.0f, 8.0f};

// ---------------------------------------------------------------------------
// Backdrop
// ---------------------------------------------------------------------------

enum class Backdrop : int { Checker = 0, Dark, PitchGreen, Magenta };

const char* const kBackdropLabels[] = {"Checkerboard", "Dark", "Pitch green",
                                       "Magenta (key-colour bleed)"};

void FillBackdrop(SDL_Renderer* r, const SDL_FRect& rect, Backdrop b) {
    switch (b) {
    case Backdrop::Dark:
        SDL_SetRenderDrawColor(r, 26, 26, 32, 255);
        SDL_RenderFillRect(r, &rect);
        return;
    case Backdrop::PitchGreen:
        SDL_SetRenderDrawColor(r, 20, 90, 40, 255);
        SDL_RenderFillRect(r, &rect);
        return;
    case Backdrop::Magenta:
        // Anything that survives against magenta is either real art or a hole
        // where the index-0 key colour did not take.
        SDL_SetRenderDrawColor(r, 255, 0, 255, 255);
        SDL_RenderFillRect(r, &rect);
        return;
    case Backdrop::Checker:
        break;
    }

    SDL_SetRenderDrawColor(r, 48, 48, 54, 255);
    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, 64, 64, 72, 255);
    constexpr float kCell = 8.0f;
    for (float y = rect.y; y < rect.y + rect.h; y += kCell) {
        for (float x = rect.x; x < rect.x + rect.w; x += kCell) {
            const int cx = static_cast<int>((x - rect.x) / kCell);
            const int cy = static_cast<int>((y - rect.y) / kCell);
            if (((cx + cy) & 1) == 0) continue;
            SDL_FRect cell{x, y, kCell, kCell};
            if (cell.x + cell.w > rect.x + rect.w) cell.w = rect.x + rect.w - cell.x;
            if (cell.y + cell.h > rect.y + rect.h) cell.h = rect.y + rect.h - cell.y;
            SDL_RenderFillRect(r, &cell);
        }
    }
}

// ---------------------------------------------------------------------------
// Which existing table, if any, already claims a frame
// ---------------------------------------------------------------------------
//
// Raw mode's real question is "is this frame spoken for?". Answering it from the
// tables themselves means the answer cannot drift from animation_tables.hpp.

struct NamedTable {
    const char*              name;
    const at::anim::DirTable* table;
};

const NamedTable kNamedTables[] = {
    {"kRunning", &at::anim::kRunning},
    {"kStanding", &at::anim::kStanding},
    {"kSliding", &at::anim::kSliding},
    {"kKeeperRunning", &at::anim::kKeeperRunning},
};

std::string FrameUsage(int frame) {
    std::string out;
    for (const NamedTable& nt : kNamedTables) {
        for (int d = 0; d < at::anim::kDirections; ++d) {
            const at::anim::FrameList list = at::anim::At(*nt.table, d);
            bool hit = false;
            for (int16_t v : list) {
                if (v >= 0 && v == static_cast<int16_t>(frame)) { hit = true; break; }
            }
            if (!hit) continue;
            if (!out.empty()) out += ", ";
            out += nt.name;
            out += '[';
            out += at::spriteview::DirectionName(d);
            out += ']';
        }
    }
    // The two prone frames are direction-independent and live outside a DirTable.
    if (frame == 55) { if (!out.empty()) out += ", "; out += "kDownNorthward"; }
    if (frame == 54) { if (!out.empty()) out += ", "; out += "kDownSouthward"; }
    return out;
}

// ---------------------------------------------------------------------------
// Kit dialog helpers
// ---------------------------------------------------------------------------

ImVec4 KitColourSwatch(const at::GamePalette& pal, std::span<const uint8_t> ordinals,
                       uint8_t kit_colour) {
    const auto& table = ordinals.size() >= at::kKitColourCount
                            ? ordinals
                            : std::span<const uint8_t>(at::render::kFallbackKitOrdinals);
    const uint8_t idx = table[kit_colour < at::kKitColourCount ? kit_colour : 0];
    if (idx < pal.count && (static_cast<size_t>(idx) * 4 + 3) < pal.rgba.size()) {
        return ImVec4(pal.rgba[idx * 4 + 0] / 255.0f, pal.rgba[idx * 4 + 1] / 255.0f,
                      pal.rgba[idx * 4 + 2] / 255.0f, 1.0f);
    }
    return ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
}

// Ten swatches, not a combo of integers: the whole point of picking a kit colour
// is seeing it. Returns true when the value changed.
bool KitColourRow(const char* label, uint8_t& value, const at::GamePalette& pal,
                  std::span<const uint8_t> ordinals) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(110.0f);
    for (uint8_t c = 0; c < at::kKitColourCount; ++c) {
        if (c) ImGui::SameLine(0.0f, 2.0f);
        ImGui::PushID(c);
        const bool selected = (value == c);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, selected ? 3.0f : 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border,
                              selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                                       : ImVec4(0.0f, 0.0f, 0.0f, 0.6f));
        if (ImGui::ColorButton("##sw", KitColourSwatch(pal, ordinals, c),
                               ImGuiColorEditFlags_NoTooltip |
                                   ImGuiColorEditFlags_NoDragDrop,
                               ImVec2(20, 20))) {
            value   = c;
            changed = true;
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::SetItemTooltip("kit colour %u", unsigned(c));
        ImGui::PopID();
    }
    ImGui::PopID();
    return changed;
}

// ---------------------------------------------------------------------------
// Bookmarks
// ---------------------------------------------------------------------------

struct Bookmark {
    int  frame = 0;
    char note[48] = {};
};

// The output the tool is actually for: a frame list you can paste into
// core/animation_tables.hpp once you have read the frames off the art.
std::string BookmarksAsFrameList(const std::vector<Bookmark>& marks, bool loop) {
    std::string out = "inline constexpr int16_t kNewList[] = {";
    for (size_t i = 0; i < marks.size(); ++i) {
        if (i) out += ", ";
        out += std::to_string(marks[i].frame);
    }
    if (!marks.empty()) out += ", ";
    out += loop ? "kLoop};" : "kHold};";
    return out;
}

} // namespace

int main(int argc, char** argv) {
    const char* assets_dir      = AT_ASSET_DIR "/generated";
    const char* placeholder_dir = AT_ASSET_DIR "/placeholder";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
            assets_dir = argv[++i];
        } else {
            std::fprintf(stderr, "usage: %s [--assets <dir>]\n", argv[0]);
            return 2;
        }
    }

    std::unique_ptr<at::IAssetSource> assets =
        at::OpenAssetSource(assets_dir, placeholder_dir);
    if (!assets) {
        std::fprintf(stderr, "sprite_viewer: no asset source (missing %s?)\n",
                     placeholder_dir);
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("aftertouch sprite_viewer", kWindowW, kWindowH,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        std::fprintf(stderr, "window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    // Keyboard nav would let Tab (and the arrow keys) walk panel widgets, which
    // in a tool driven by held arrow keys means the preview silently stops
    // responding the moment a widget takes focus.
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // --- model ------------------------------------------------------------
    Preview                 preview;
    at::spriteview::Config  cfg;
    at::spriteview::Reset(preview);

    Mode     mode     = Mode::Table;
    Backdrop backdrop = Backdrop::Checker;
    float    mags[3]  = {kDefaultMags[0], kDefaultMags[1], kDefaultMags[2]};

    bool show_anchor = true;
    bool show_bbox   = true;
    bool show_grid   = true;

    bool paused    = false;
    int  ticks_hz  = 50;   // MatchEngine::kTickHz — preview at match speed
    bool step_once = false;

    // Vertical stripes: the one geometry that shows both shirt colours at once,
    // so the kit dialog has something to say from the first frame.
    preview.shirt_type = 2;

    at::KitSpec kit{};
    kit.shirt   = 1;
    kit.stripes = 0;
    kit.shorts  = 0;
    kit.socks   = 1;
    int face = 0;

    at::render::KitPalette palette{};
    bool                   palette_dirty = true;

    std::vector<Bookmark> bookmarks;
    bool                  bookmark_loop = true;

    // Probe how many frames the loaded pack really serves. The constants say 101
    // and 58; a partial pack says otherwise, and silently drawing nothing would
    // read as an art bug.
    auto probe_frames = [&](bool keeper, at::ShirtGeometry geo) {
        int n = 0;
        const int cap = keeper ? at::kKeeperFrameCount : at::kPlayerFrameCount;
        for (int f = 0; f < cap; ++f) {
            const at::SpriteSheet* s =
                keeper ? assets->Keeper(f) : assets->Player(geo, f);
            if (s && s->width) n = f + 1;
        }
        return n;
    };
    int available_frames =
        probe_frames(preview.keeper, at::spriteview::Geometry(preview));
    bool  probe_dirty    = false;

    uint64_t last_ns     = SDL_GetTicksNS();
    uint64_t accumulator = 0;

    bool running = true;
    while (running) {
        bool fire_edge = false;
        bool mood_edge = false;
        int  raw_step  = 0;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT) running = false;
            if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat &&
                !ImGui::GetIO().WantCaptureKeyboard) {
                switch (ev.key.key) {
                case SDLK_ESCAPE:       running = false; break;
                case SDLK_SPACE:        fire_edge = true; break;
                case SDLK_H:            mood_edge = true; break;
                case SDLK_LEFTBRACKET:  raw_step = -1; break;
                case SDLK_RIGHTBRACKET: raw_step = +1; break;
                // Not Tab: Tab moves ImGui's keyboard focus into a panel widget,
                // after which WantCaptureKeyboard is true and every preview key
                // is swallowed as text by whatever got focused.
                case SDLK_M:
                    mode = (mode == Mode::Table) ? Mode::Raw : Mode::Table;
                    break;
                default: break;
                }
            }
        }

        // --- input -> controls --------------------------------------------
        at::spriteview::Controls controls;
        if (!ImGui::GetIO().WantCaptureKeyboard) {
            const bool* k = SDL_GetKeyboardState(nullptr);
            const bool up = k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W] ||
                            k[SDL_SCANCODE_KP_8] || k[SDL_SCANCODE_KP_7] ||
                            k[SDL_SCANCODE_KP_9];
            const bool down = k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S] ||
                              k[SDL_SCANCODE_KP_2] || k[SDL_SCANCODE_KP_1] ||
                              k[SDL_SCANCODE_KP_3];
            const bool left = k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A] ||
                              k[SDL_SCANCODE_KP_4] || k[SDL_SCANCODE_KP_7] ||
                              k[SDL_SCANCODE_KP_1];
            const bool right = k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D] ||
                               k[SDL_SCANCODE_KP_6] || k[SDL_SCANCODE_KP_9] ||
                               k[SDL_SCANCODE_KP_3];
            controls.direction = at::spriteview::DirectionFromAxes(up, down, left, right);
        }
        controls.fire_pressed = fire_edge;
        controls.mood_pressed = mood_edge;

        if (raw_step != 0) {
            mode = Mode::Raw;
            preview.raw_frame = static_cast<int16_t>(preview.raw_frame + raw_step);
        }

        // --- fixed-rate stepping ------------------------------------------
        const uint64_t now_ns = SDL_GetTicksNS();
        uint64_t frame_ns = now_ns - last_ns;
        last_ns = now_ns;
        if (frame_ns > 250'000'000ull) frame_ns = 250'000'000ull;

        at::spriteview::Apply(preview, controls, cfg);

        if (paused) {
            accumulator = 0;
            if (step_once) {
                at::spriteview::Tick(preview, mode);
                step_once = false;
            }
        } else {
            const uint64_t tick_ns =
                1'000'000'000ull / static_cast<uint64_t>(ticks_hz < 1 ? 1 : ticks_hz);
            accumulator += frame_ns;
            int guard = 0;
            while (accumulator >= tick_ns && guard++ < 64) {
                accumulator -= tick_ns;
                at::spriteview::Tick(preview, mode);
            }
        }

        // --- resolve the sheet to draw ------------------------------------
        if (probe_dirty) {
            available_frames =
                probe_frames(preview.keeper, at::spriteview::Geometry(preview));
            probe_dirty = false;
        }
        if (palette_dirty) {
            // One source of truth for the shirt type: the preview owns it because
            // it also selects the bank, and BuildKitPalette needs the same value
            // to decide whether the stripe colour collapses into the shirt.
            kit.shirt_type = preview.shirt_type;
            at::render::BuildKitPalette(assets->Palette(), assets->KitColourOrdinals(),
                                        kit, static_cast<uint8_t>(face), palette);
            // The texture cache keys on the palette's address, which has not
            // changed — only its contents have. Without this the preview would
            // keep drawing the previous kit.
            at::render::InvalidateIndexedSpriteCache();
            palette_dirty = false;
        }

        const int frame = preview.e.image_index;
        const at::SpriteSheet* sheet = nullptr;
        if (frame >= 0) {
            sheet = preview.keeper
                        ? assets->Keeper(frame)
                        : assets->Player(at::spriteview::Geometry(preview), frame);
        }

        // --- draw ---------------------------------------------------------
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Two coordinate spaces meet here. ImGui lays out in points; SDL draws in
        // device pixels; on a scaled display they differ. Take ImGui's DisplaySize
        // as the authority for *layout* and convert to pixels for the SDL half,
        // so the panel and the preview agree at any DPI. Magnification stays in
        // pixels on purpose: 8x means eight device pixels per texel, which is the
        // only reading of "8x" a sprite viewer is allowed to have.
        const ImGuiIO& io = ImGui::GetIO();
        int px_w = 0, px_h = 0;
        SDL_GetRenderOutputSize(renderer, &px_w, &px_h);
        const float sx = io.DisplaySize.x > 0.0f
                             ? static_cast<float>(px_w) / io.DisplaySize.x
                             : 1.0f;
        const float sy = io.DisplaySize.y > 0.0f
                             ? static_cast<float>(px_h) / io.DisplaySize.y
                             : 1.0f;
        const float view_w = (io.DisplaySize.x - static_cast<float>(kPanelW)) * sx;
        const float view_h = io.DisplaySize.y * sy;

        SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
        SDL_RenderClear(renderer);

        if (view_w > 40.0f) {
            // Three panes side by side, each sized to its magnification and
            // vertically centred on one shared baseline so anchor drift between
            // magnifications shows up as a visible step rather than a guess.
            float total = 0.0f;
            float pane_w[3];
            for (int i = 0; i < 3; ++i) {
                pane_w[i] = at::kPlayerSpriteW * mags[i] + 48.0f;
                if (pane_w[i] < 90.0f) pane_w[i] = 90.0f;
                total += pane_w[i];
            }
            const float gap = 16.0f;
            total += gap * 2.0f;

            float x = (view_w - total) * 0.5f;
            if (x < 8.0f) x = 8.0f;

            // One shared pane height, tall enough for the biggest magnification.
            // Shared rather than per-pane so the three sprites sit on one baseline
            // and anchor drift between magnifications reads as a visible step.
            float max_mag = mags[0];
            for (int i = 1; i < 3; ++i)
                if (mags[i] > max_mag) max_mag = mags[i];
            float pane_h = at::kPlayerSpriteH * max_mag + 80.0f;
            const float pane_limit = view_h - 100.0f;
            if (pane_h > pane_limit) pane_h = pane_limit;
            if (pane_h < 120.0f) pane_h = 120.0f;
            const float pane_y = (view_h - pane_h) * 0.5f;

            for (int i = 0; i < 3; ++i) {
                const SDL_FRect pane{x, pane_y, pane_w[i], pane_h};
                FillBackdrop(renderer, pane, backdrop);
                SDL_SetRenderDrawColor(renderer, 110, 110, 120, 255);
                SDL_RenderRect(renderer, &pane);

                const float cx = pane.x + pane.w * 0.5f;
                const float cy = pane.y + pane.h * 0.5f;

                if (sheet && sheet->width) {
                    if (show_grid && mags[i] >= 4.0f) {
                        const float ax = static_cast<float>(sheet->anchor_x) * mags[i];
                        const float ay = static_cast<float>(sheet->anchor_y) * mags[i];
                        const float x0 = cx - ax, y0 = cy - ay;
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 28);
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                        for (int gx = 0; gx <= sheet->width; ++gx) {
                            const float px = x0 + static_cast<float>(gx) * mags[i];
                            SDL_RenderLine(renderer, px, y0, px,
                                           y0 + sheet->height * mags[i]);
                        }
                        for (int gy = 0; gy <= sheet->height; ++gy) {
                            const float py = y0 + static_cast<float>(gy) * mags[i];
                            SDL_RenderLine(renderer, x0, py,
                                           x0 + sheet->width * mags[i], py);
                        }
                    }

                    at::render::DrawIndexedSprite(renderer, *sheet, palette.Bytes(),
                                                  at::render::kPaletteEntries, cx, cy,
                                                  mags[i]);

                    if (show_bbox) {
                        const SDL_FRect bb{cx - sheet->anchor_x * mags[i],
                                           cy - sheet->anchor_y * mags[i],
                                           sheet->width * mags[i],
                                           sheet->height * mags[i]};
                        SDL_SetRenderDrawColor(renderer, 90, 200, 255, 160);
                        SDL_RenderRect(renderer, &bb);
                    }
                    if (show_anchor) {
                        // The anchor is where DrawIndexedSprite places the world
                        // point, so it is the same pixel in all three panes.
                        SDL_SetRenderDrawColor(renderer, 255, 80, 80, 220);
                        SDL_RenderLine(renderer, cx - 6.0f, cy, cx + 6.0f, cy);
                        SDL_RenderLine(renderer, cx, cy - 6.0f, cx, cy + 6.0f);
                    }
                }

                char label[32];
                std::snprintf(label, sizeof(label), "%gx", static_cast<double>(mags[i]));
                SDL_SetRenderDrawColor(renderer, 235, 235, 235, 255);
                SDL_RenderDebugText(renderer, pane.x + 6.0f, pane.y - 16.0f, label);

                x += pane_w[i] + gap;
            }

            char status[192];
            std::snprintf(status, sizeof(status),
                          "%s  dir=%s  frame=%d  frame_index=%d  switches=%d  %s",
                          at::spriteview::StateName(preview),
                          at::spriteview::DirectionName(preview.e.direction),
                          int(preview.e.image_index), int(preview.e.frame_index),
                          int(preview.e.frame_switch_counter),
                          mode == Mode::Raw ? "[RAW]" : "");
            SDL_SetRenderDrawColor(renderer, 235, 235, 235, 255);
            SDL_RenderDebugText(renderer, 12.0f, view_h - 26.0f, status);
        }

        // --- panel --------------------------------------------------------
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x - static_cast<float>(kPanelW), 0.0f));
        ImGui::SetNextWindowSize(
            ImVec2(static_cast<float>(kPanelW), io.DisplaySize.y));
        ImGui::Begin("sprite_viewer", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        if (assets->IsPlaceholder()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.35f, 1.0f));
            ImGui::TextWrapped(
                // ASCII only in anything ImGui renders: the default font has no
                // glyph for an em dash and draws it as '?'.
                "PLACEHOLDER ART - synthesised shapes, not the imported bank. "
                "Do not measure frame taxonomy from this. Run assetc into %s first.",
                assets_dir);
            ImGui::PopStyleColor();
        } else {
            ImGui::TextDisabled("assets: %s", assets_dir);
        }
        ImGui::Text("bank serves %d frames (constant says %d)", available_frames,
                    preview.keeper ? at::kKeeperFrameCount : at::kPlayerFrameCount);

        ImGui::SeparatorText("Mode");
        int mode_i = (mode == Mode::Table) ? 0 : 1;
        if (ImGui::RadioButton("Table (what the game draws)", &mode_i, 0))
            mode = Mode::Table;
        if (ImGui::RadioButton("Raw (scrub the bank)", &mode_i, 1)) mode = Mode::Raw;

        ImGui::SeparatorText("State");
        ImGui::Text("%s  facing %s", at::spriteview::StateName(preview),
                    at::spriteview::DirectionName(preview.e.direction));
        ImGui::Text("image_index %d   frame_index %d   delay %d   switches %d",
                    int(preview.e.image_index), int(preview.e.frame_index),
                    int(preview.e.frame_delay), int(preview.e.frame_switch_counter));
        if (mode == Mode::Table && !at::spriteview::StateHasTable(preview)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.3f, 1.0f));
            ImGui::TextWrapped(
                "No frame table for this state yet - SelectAnimationTable falls "
                "through to standing. C3 has 30..53 and 64..100 unclassified; use "
                "Raw mode to find them.");
            ImGui::PopStyleColor();
        }
        ImGui::Text("next fire: %s     next mood: %s",
                    preview.next_oneshot == at::spriteview::OneShot::Header
                        ? "header"
                        : "slide",
                    preview.next_mood == at::spriteview::Mood::Happy ? "happy" : "sad");
        if (preview.oneshot_timer > 0)
            ImGui::Text("one-shot ends in %d ticks", int(preview.oneshot_timer));

        ImGui::SeparatorText("Playback");
        ImGui::Checkbox("Paused", &paused);
        ImGui::SameLine();
        if (ImGui::Button("Step")) { paused = true; step_once = true; }
        ImGui::SliderInt("ticks/sec", &ticks_hz, 1, 60);
        ImGui::SetItemTooltip("The match runs at 50. Lower to read a cycle frame by frame.");
        int hold = cfg.oneshot_ticks;
        if (ImGui::SliderInt("one-shot ticks", &hold, 1, 200))
            cfg.oneshot_ticks = static_cast<int16_t>(hold);
        ImGui::SetItemTooltip(
            "Header/slide/mood lists end in kHold and freeze forever. This is how "
            "long the viewer holds them before returning to idle.");

        if (mode == Mode::Raw) {
            ImGui::SeparatorText("Raw frame");
            int rf = preview.raw_frame;
            const int cap =
                (available_frames > 0 ? available_frames
                                      : (preview.keeper ? at::kKeeperFrameCount
                                                        : at::kPlayerFrameCount)) - 1;
            if (ImGui::SliderInt("frame", &rf, 0, cap > 0 ? cap : 0))
                preview.raw_frame = static_cast<int16_t>(rf);
            if (ImGui::Button("< prev")) preview.raw_frame--;
            ImGui::SameLine();
            if (ImGui::Button("next >")) preview.raw_frame++;
            ImGui::SameLine();
            ImGui::TextDisabled("or [ and ]");

            const std::string usage = FrameUsage(preview.raw_frame);
            if (usage.empty())
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f),
                                   "frame %d: unclaimed", int(preview.raw_frame));
            else
                ImGui::TextWrapped("frame %d: %s", int(preview.raw_frame), usage.c_str());

            ImGui::SeparatorText("Bookmarks");
            if (ImGui::Button("Add current")) {
                Bookmark b;
                b.frame = preview.raw_frame;
                bookmarks.push_back(b);
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear")) bookmarks.clear();
            ImGui::SameLine();
            ImGui::Checkbox("loop", &bookmark_loop);
            ImGui::SetItemTooltip("Terminate the exported list with kLoop, else kHold.");

            for (size_t i = 0; i < bookmarks.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::SmallButton("go"))
                    preview.raw_frame = static_cast<int16_t>(bookmarks[i].frame);
                ImGui::SameLine();
                ImGui::Text("%3d", bookmarks[i].frame);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(190.0f);
                ImGui::InputText("##note", bookmarks[i].note, sizeof(bookmarks[i].note));
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) {
                    bookmarks.erase(bookmarks.begin() + static_cast<ptrdiff_t>(i));
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }

            if (!bookmarks.empty()) {
                const std::string code = BookmarksAsFrameList(bookmarks, bookmark_loop);
                ImGui::TextWrapped("%s", code.c_str());
                if (ImGui::Button("Copy frame list"))
                    ImGui::SetClipboardText(code.c_str());
                ImGui::SetItemTooltip("Paste into core/animation_tables.hpp.");
            }
        }

        ImGui::SeparatorText("Kit");
        {
            const at::GamePalette pal = assets->Palette();
            const auto ordinals       = assets->KitColourOrdinals();

            int shirt_type = preview.shirt_type;
            const char* kShirtTypes[] = {"0 plain", "1 coloured sleeves",
                                         "2 vertical stripes", "3 horizontal stripes"};
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::Combo("shirt type", &shirt_type, kShirtTypes, 4)) {
                preview.shirt_type = static_cast<uint8_t>(shirt_type);
                palette_dirty      = true;   // plain collapses the stripe colour
                probe_dirty        = true;   // and it selects a different bank
            }
            ImGui::TextDisabled("bank: %s",
                                at::spriteview::Geometry(preview) ==
                                        at::ShirtGeometry::HorizontalStripes
                                    ? "kit_hstripe"
                                    : (at::spriteview::Geometry(preview) ==
                                               at::ShirtGeometry::ColouredSleeves
                                           ? "kit_sleeves"
                                           : "kit_vstripe"));

            if (KitColourRow("shirt", kit.shirt, pal, ordinals)) palette_dirty = true;
            if (KitColourRow("stripes", kit.stripes, pal, ordinals)) palette_dirty = true;
            if (KitColourRow("shorts", kit.shorts, pal, ordinals)) palette_dirty = true;
            if (KitColourRow("socks", kit.socks, pal, ordinals)) palette_dirty = true;
            if (kit.shirt_type == 0)
                ImGui::TextDisabled("(plain: stripe colour is ignored)");

            ImGui::SeparatorText("Face / hair");
            const char* kFaces[] = {"0 - as imported", "1 - ginger hair",
                                    "2 - dark skin + hair"};
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::Combo("face", &face, kFaces, at::render::kFaceCount))
                palette_dirty = true;
            // TextDisabled does not wrap, so a sentence this long would be
            // silently clipped by the panel edge.
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            ImGui::TextWrapped("Faces are palette-only (C3 Finding 1) - the bank "
                               "must not change here.");
            ImGui::PopStyleColor();

            if (ImGui::Checkbox("Goalkeeper bank", &preview.keeper)) {
                preview.e.player_ordinal = preview.keeper ? 1 : 2;
                probe_dirty              = true;
                if (preview.raw_frame >= at::kKeeperFrameCount && preview.keeper)
                    preview.raw_frame = at::kKeeperFrameCount - 1;
            }
            ImGui::SetItemTooltip(
                "58 frames from a 116-slot band; whether that is 58 real frames or "
                "two copies is still a C3 open question.");
        }

        ImGui::SeparatorText("View");
        ImGui::SliderFloat("mag 1", &mags[0], 1.0f, 16.0f, "%.0fx");
        ImGui::SliderFloat("mag 2", &mags[1], 1.0f, 16.0f, "%.0fx");
        ImGui::SliderFloat("mag 3", &mags[2], 1.0f, 16.0f, "%.0fx");
        int bd = static_cast<int>(backdrop);
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::Combo("backdrop", &bd, kBackdropLabels, 4))
            backdrop = static_cast<Backdrop>(bd);
        ImGui::Checkbox("anchor", &show_anchor);
        ImGui::SameLine();
        ImGui::Checkbox("bbox", &show_bbox);
        ImGui::SameLine();
        ImGui::Checkbox("pixel grid", &show_grid);

        ImGui::SeparatorText("Controls");
        ImGui::TextDisabled(
            "arrows / WASD / numpad  steer (diagonals)\n"
            "space                   header <-> slide tackle\n"
            "H                       happy <-> sad\n"
            "[ ]                     step frame (raw mode)\n"
            "M                       toggle table / raw mode\n"
            "esc                     quit");
        if (ImGui::Button("Reset entity")) at::spriteview::Reset(preview);

        ImGui::End();

        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    at::render::InvalidateIndexedSpriteCache();
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
