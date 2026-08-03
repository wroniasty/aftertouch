#pragma once
#include "core/match_state.hpp"
#include "render/pitch_view.hpp"

#include <cstdint>

// C2 — the camera.
//
// Deliberately OUTSIDE at_core (CAMERA.md §11): it reads simulation state and affects
// nothing, so keeping it here means a camera change can never break replay
// determinism. The one exception is the kick-off end, which is a draw from the match
// RNG in the original; we take it from `presentation_rng`, which HashState already
// excludes, so the same reasoning holds without giving up the coin flip.
//
// The model is the reference's and is worth copying as structure: each mode returns a
// destination, a side margin and a lead offset, the priority list picks one, and one
// shared routine eases and clips. The numbers are pitch geometry, not taste, and are
// named here rather than sprinkled through the update.

namespace at::render {

// Camera position is the TOP-LEFT of the 320×200 window in the 672×880 pitch world, so
// its travel is exactly world minus window on each axis.
inline constexpr int16_t kCameraMinX = 0;
inline constexpr int16_t kCameraMaxX = 352;   // 672 − 320
inline constexpr int16_t kCameraMinY = 16;
inline constexpr int16_t kCameraMaxY = 680;   // 880 − 200

// Destination bounds are tighter than position bounds, and not by a typo: the
// destination is where the camera wants to be, the position clip is the hard wall, and
// the camera may sit briefly where its destination was never allowed (CAMERA.md §7).
inline constexpr int16_t kDestMinY = 16;
inline constexpr int16_t kDestMaxY = 664;

inline constexpr int16_t kSideLimitInPlay = 63;   // never show the very touchline
inline constexpr int16_t kSideLimitBreak  = 37;   // corners and throw-ins may
inline constexpr int16_t kSideLimitSub    = 51;

inline constexpr int16_t kLeadStep    = 2;    // per tick
inline constexpr int16_t kLeadMax     = 40;   // 20 ticks to full lead
inline constexpr int16_t kEaseShift   = 4;    // /16 of the remaining distance
inline constexpr int16_t kMaxStep     = 5;    // per-tick movement ceiling

inline constexpr int16_t kKickoffX      = 176;
inline constexpr int16_t kKickoffTopY   = 16;
inline constexpr int16_t kKickoffBotY   = 664;
inline constexpr int16_t kPenaltyCamX   = 336 - kLogicalW / 2;
inline constexpr int16_t kPenaltyCamY   = 107;
inline constexpr int16_t kWalkOffX      = 590;
inline constexpr int16_t kResultY       = 129;

enum class CameraMode : uint8_t {
    FollowBall = 0,
    Booking,
    PenaltyShootout,
    WalkOff,      // players leaving the pitch
    Result,
    Centre,
    Frozen,
};

struct CameraParams {
    int16_t dest_x     = 0;
    int16_t dest_y     = 0;
    int16_t side_limit = kSideLimitInPlay;
    int16_t lead_x     = 0;
    int16_t lead_y     = 0;
    bool    frozen     = false;
};

class Camera {
public:
    // One coin flip: the kick-off camera starts at a random end and eases in.
    // `roll` comes from the caller's presentation RNG (MatchEngine::DrawPresentationRng).
    void Reset(uint16_t roll);

    // Advance one tick against the current match state.
    void Update(const MatchState& state);

    // Snap to the destination — for a restart or a mode change that should not pan.
    void SnapToDestination(const MatchState& state);

    // The 320×200 window this camera is looking at.
    DebugView View() const;

    CameraMode Mode() const { return mode_; }
    int16_t    X() const { return x_; }
    int16_t    Y() const { return y_; }
    int16_t    LeadX() const { return lead_x_; }
    int16_t    LeadY() const { return lead_y_; }

private:
    CameraParams Resolve(const MatchState& state) const;
    void         Apply(const CameraParams& p);

    int16_t    x_      = kKickoffX;
    int16_t    y_      = kKickoffTopY;
    int16_t    lead_x_ = 0;
    int16_t    lead_y_ = 0;
    CameraMode mode_   = CameraMode::FollowBall;
};

// Pure helpers, exposed for tests.
int16_t ClipCameraDestination(int32_t v, int16_t lo, int16_t hi);
int16_t EaseTowards(int16_t from, int16_t to);
// Ramp one lead axis toward `direction` (sign only), by kLeadStep, capped at kLeadMax.
int16_t RampLead(int16_t lead, int direction);

} // namespace at::render
