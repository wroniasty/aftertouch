#pragma once
#include <cstdint>

// Platform profile — Amiga/50 Hz is the default oracle.
// See doc/implementation/A2-determinism-primitives.md section 2.4a.
//
// A2 owns the trig half (the 41/64 PC compensation in the movement kernel).
// B3 extends with ball physics constants; B2 with timer intervals and the clock.

namespace at {

enum class PlatformProfile : uint8_t {
    Amiga = 0,
    Pc    = 1,
};

// Compile-time selection. Flip to Pc only for deliberate A/B; a silent flip
// rescales every speed by ~0.64 and reads as a tuning problem.
inline constexpr PlatformProfile kPlatformProfile = PlatformProfile::Amiga;

inline constexpr bool IsAmigaProfile() {
    return kPlatformProfile == PlatformProfile::Amiga;
}

// Ball physics constants — BALL.md §9 / B3. Amiga is the default oracle.
inline constexpr int16_t kBallGroundConstant =
    IsAmigaProfile() ? int16_t{16} : int16_t{13};
inline constexpr int16_t kBallAirConstant =
    IsAmigaProfile() ? int16_t{10} : int16_t{4};
inline constexpr int32_t kGravityConstant =
    IsAmigaProfile() ? 4608 : 3291;

} // namespace at
