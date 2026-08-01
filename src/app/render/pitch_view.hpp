#pragma once
#include <cstdint>

// Pure pitch→screen mapper for the C1a debug view. No SDL.
// See doc/implementation/C1a-debug-match-view.md.

namespace at::render {

inline constexpr int kLogicalW = 320;
inline constexpr int kLogicalH = 200;

// Debug frustum: dead-ball / pitch frame (BALL.md barrier box).
inline constexpr int16_t kViewMinX = 53;
inline constexpr int16_t kViewMaxX = 618;
inline constexpr int16_t kViewMinY = 100;
inline constexpr int16_t kViewMaxY = 799;

struct ScreenPos {
    int x = 0;
    int y = 0;
};

// Map whole pitch units into 320×200, letterboxed to preserve aspect.
inline ScreenPos PitchToScreen(int16_t pitch_x, int16_t pitch_y,
                               int match_w = kLogicalW, int match_h = kLogicalH) {
    const float world_w = static_cast<float>(kViewMaxX - kViewMinX);
    const float world_h = static_cast<float>(kViewMaxY - kViewMinY);
    const float scale_x = static_cast<float>(match_w) / world_w;
    const float scale_y = static_cast<float>(match_h) / world_h;
    const float scale = (scale_x < scale_y) ? scale_x : scale_y;
    const float used_w = world_w * scale;
    const float used_h = world_h * scale;
    const float ox = (static_cast<float>(match_w) - used_w) * 0.5f;
    const float oy = (static_cast<float>(match_h) - used_h) * 0.5f;

    const float nx = (static_cast<float>(pitch_x) - static_cast<float>(kViewMinX)) * scale;
    const float ny = (static_cast<float>(pitch_y) - static_cast<float>(kViewMinY)) * scale;
    ScreenPos s;
    s.x = static_cast<int>(ox + nx + 0.5f);
    s.y = static_cast<int>(oy + ny + 0.5f);
    if (s.x < 0) s.x = 0;
    if (s.y < 0) s.y = 0;
    if (s.x >= match_w) s.x = match_w - 1;
    if (s.y >= match_h) s.y = match_h - 1;
    return s;
}

} // namespace at::render
