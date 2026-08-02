#pragma once

struct SDL_Renderer;

namespace at {

struct MatchState;

namespace render {

// Draws the match at the sacred logical resolution. The caller is responsible
// for setting integer-scale logical presentation before this and disabling it
// after (see PLAN.md section 6 "Two presentation modes in one renderer").
// follow_ball: sliding zoomed window (default); false = whole dead-ball box.
void DrawMatch(SDL_Renderer* r, const MatchState& state, int match_w, int match_h,
               bool follow_ball = true);

// Debug HUD in *window* pixels (call after logical presentation is disabled)
// so text is not blown up by the 320×200 integer scale.
void DrawMatchHud(SDL_Renderer* r, const MatchState& state, bool follow_ball = true,
                  int sim_speed_pct = 100);

} // namespace render
} // namespace at
