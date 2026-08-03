#pragma once

struct SDL_Renderer;

namespace at {

struct MatchState;
class IAssetSource;

namespace tracekit {
struct KickTelemetry;
}

namespace render {

class PitchAtlas;
class KitBank;
class Camera;

// Draws the match at the sacred logical resolution. The caller is responsible
// for setting integer-scale logical presentation before this and disabling it
// after (see PLAN.md section 6 "Two presentation modes in one renderer").
// camera: C2 view; null falls back to the whole dead-ball box.
// assets / atlas: tiled pitch + sprites; null falls back to flat green + dots (C1a).
// kits: C3 per-team palettes; null draws the art in its imported colours.
void DrawMatch(SDL_Renderer* r, const MatchState& state, int match_w, int match_h,
               const Camera* camera = nullptr, IAssetSource* assets = nullptr,
               PitchAtlas* atlas = nullptr, const KitBank* kits = nullptr);

// Debug HUD in *window* pixels (call after logical presentation is disabled)
// so text is not blown up by the 320×200 integer scale. C3 put this behind F1:
// it is instrumentation, not chrome, but B6a's kick-timing work reads it, so it
// is toggled rather than deleted.
// mode_tag: short label for a non-standard match (C1b sandbox); null when off.
// kick: last strike's control telemetry (B6a); null hides the control line.
void DrawMatchHud(SDL_Renderer* r, const MatchState& state, const Camera* camera,
                  int sim_speed_pct = 100, const char* mode_tag = nullptr,
                  const tracekit::KickTelemetry* kick = nullptr);

// One-line status always shown: clock, score, phase. No instrumentation.
void DrawMatchStatus(SDL_Renderer* r, const MatchState& state, const char* mode_tag);

} // namespace render
} // namespace at
