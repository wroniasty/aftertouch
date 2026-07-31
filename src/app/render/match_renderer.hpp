#pragma once

struct SDL_Renderer;

namespace at {

struct MatchState;

namespace render {

// Draws the match at the sacred logical resolution. The caller is responsible
// for setting integer-scale logical presentation before this and disabling it
// after (see PLAN.md section 6 "Two presentation modes in one renderer").
//
// Milestone 1 draws a placeholder pitch and a debug overlay; this grows into
// real sprite drawing in Phase 1.
void DrawMatch(SDL_Renderer* r, const MatchState& state, int match_w, int match_h);

} // namespace render
} // namespace at
