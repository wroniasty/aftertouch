#pragma once
#include <cstdint>

// Pure pitch→screen mapper for the C1a debug view. No SDL.
// See doc/implementation/C1a-debug-match-view.md.

namespace at::render {

inline constexpr int kLogicalW = 320;
inline constexpr int kLogicalH = 200;

// Dead-ball / pitch frame (BALL.md barrier box) — full-pitch debug frustum.
inline constexpr int16_t kViewMinX = 53;
inline constexpr int16_t kViewMaxX = 618;
inline constexpr int16_t kViewMinY = 100;
inline constexpr int16_t kViewMaxY = 799;

// Ball-follow window. Exactly the logical frame, so 1 pitch unit = 1 pixel and
// PitchUniformScale() is 1.0: 16-px tiles land on 16-px boundaries and sprites
// blit unscaled, as in the original (PITCH.md §4 — 672×848 world of 16×16
// patterns; the reference port's zoom is a porter addition, PITCH.md §5).
// Changing these reintroduces fractional scaling and smears the tile weave.
inline constexpr int16_t kFollowWinW = kLogicalW;
inline constexpr int16_t kFollowWinH = kLogicalH;

struct ScreenPos {
    int x = 0;
    int y = 0;
};

struct ScreenPosF {
    float x = 0.f;
    float y = 0.f;
};

// Axis-aligned pitch window mapped into the logical frame.
struct DebugView {
    int16_t min_x = kViewMinX;
    int16_t max_x = kViewMaxX;
    int16_t min_y = kViewMinY;
    int16_t max_y = kViewMaxY;
};

inline DebugView FullPitchView() {
    return DebugView{kViewMinX, kViewMaxX, kViewMinY, kViewMaxY};
}

// Sliding window centred on (cx, cy), clamped inside the dead-ball box.
inline DebugView WindowAround(int16_t cx, int16_t cy,
                              int16_t win_w = kFollowWinW,
                              int16_t win_h = kFollowWinH) {
    if (win_w < 32) win_w = 32;
    if (win_h < 32) win_h = 32;
    const int16_t max_w =
        static_cast<int16_t>(kViewMaxX - kViewMinX);
    const int16_t max_h =
        static_cast<int16_t>(kViewMaxY - kViewMinY);
    if (win_w > max_w) win_w = max_w;
    if (win_h > max_h) win_h = max_h;

    int32_t min_x = static_cast<int32_t>(cx) - win_w / 2;
    int32_t min_y = static_cast<int32_t>(cy) - win_h / 2;
    if (min_x < kViewMinX) min_x = kViewMinX;
    if (min_y < kViewMinY) min_y = kViewMinY;
    if (min_x + win_w > kViewMaxX) min_x = kViewMaxX - win_w;
    if (min_y + win_h > kViewMaxY) min_y = kViewMaxY - win_h;

    DebugView v{};
    v.min_x = static_cast<int16_t>(min_x);
    v.min_y = static_cast<int16_t>(min_y);
    v.max_x = static_cast<int16_t>(min_x + win_w);
    v.max_y = static_cast<int16_t>(min_y + win_h);
    return v;
}

// Uniform pitch→screen scale (letterboxed) for a view.
inline float PitchUniformScale(const DebugView& view, int match_w = kLogicalW,
                               int match_h = kLogicalH) {
    const float world_w = static_cast<float>(view.max_x - view.min_x);
    const float world_h = static_cast<float>(view.max_y - view.min_y);
    if (world_w <= 0.f || world_h <= 0.f) return 1.f;
    const float scale_x = static_cast<float>(match_w) / world_w;
    const float scale_y = static_cast<float>(match_h) / world_h;
    return (scale_x < scale_y) ? scale_x : scale_y;
}

inline float PitchUniformScale(int match_w = kLogicalW, int match_h = kLogicalH) {
    return PitchUniformScale(FullPitchView(), match_w, match_h);
}

// Map whole pitch units into the logical frame, letterboxed to preserve aspect.
// Float form — tile drawing sizes from adjacent corners so seams do not gap.
inline ScreenPosF PitchToScreenF(int16_t pitch_x, int16_t pitch_y,
                                 const DebugView& view, int match_w = kLogicalW,
                                 int match_h = kLogicalH) {
    const float scale = PitchUniformScale(view, match_w, match_h);
    const float world_w = static_cast<float>(view.max_x - view.min_x);
    const float world_h = static_cast<float>(view.max_y - view.min_y);
    const float used_w = world_w * scale;
    const float used_h = world_h * scale;
    const float ox = (static_cast<float>(match_w) - used_w) * 0.5f;
    const float oy = (static_cast<float>(match_h) - used_h) * 0.5f;

    ScreenPosF s;
    s.x = ox + (static_cast<float>(pitch_x) - static_cast<float>(view.min_x)) * scale;
    s.y = oy + (static_cast<float>(pitch_y) - static_cast<float>(view.min_y)) * scale;
    return s;
}

// Unclamped integer mapping — used for tile corners that may sit just outside the view.
inline ScreenPos PitchToScreenUnclamped(int16_t pitch_x, int16_t pitch_y,
                                        const DebugView& view,
                                        int match_w = kLogicalW,
                                        int match_h = kLogicalH) {
    const ScreenPosF f = PitchToScreenF(pitch_x, pitch_y, view, match_w, match_h);
    ScreenPos s;
    s.x = static_cast<int>(f.x + 0.5f);
    s.y = static_cast<int>(f.y + 0.5f);
    return s;
}

inline ScreenPos PitchToScreen(int16_t pitch_x, int16_t pitch_y,
                               const DebugView& view, int match_w = kLogicalW,
                               int match_h = kLogicalH) {
    ScreenPos s = PitchToScreenUnclamped(pitch_x, pitch_y, view, match_w, match_h);
    if (s.x < 0) s.x = 0;
    if (s.y < 0) s.y = 0;
    if (s.x >= match_w) s.x = match_w - 1;
    if (s.y >= match_h) s.y = match_h - 1;
    return s;
}

inline ScreenPos PitchToScreen(int16_t pitch_x, int16_t pitch_y,
                               int match_w = kLogicalW, int match_h = kLogicalH) {
    return PitchToScreen(pitch_x, pitch_y, FullPitchView(), match_w, match_h);
}

} // namespace at::render
