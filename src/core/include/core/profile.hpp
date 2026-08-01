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

} // namespace at
