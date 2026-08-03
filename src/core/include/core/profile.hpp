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

// ---------------------------------------------------------------------------
// B13 / R5 — the six recorded disagreements between the two oracles.
// ---------------------------------------------------------------------------
//
// AMIGA_CHANGES.md §3 lists six places where the DOS-port reading and the Amiga
// reading differ. Each one changes behaviour a player notices, and each is cheap
// to settle with one targeted trace. They are **not** arbitrated here.
//
// Every switch below defaults to the *current* reading — reading A, the one the
// engine already implements — so flipping one is a single token and a scripted
// run, not a branch. Do not "fix" one by editing the default: the default moves
// when a trace says it should, and the trace is A3 item 4.
//
// Item 6 (TeamGeneralInfo +44 / +56) has no switch. Two of our three readings
// already agree with the Amiga and MOVEMENT.md §3.1 does too; AFTERTOUCH §2 is
// simply swapped and is a documentation fix, not an engine one.

// #1 — the foul-from-behind test. Reading A fouls when the two players' octants
// differ by <= 1; the Amiga fouls when they differ by > 1. Exact complements, so
// this inverts the refereeing of every challenge in the game.
inline constexpr bool kFoulFromBehindInverted = false;

// #2 — the aftertouch side latch. Reading A computes (joystick - kick) & 7; the
// Amiga computes (kick - joystick) & 7. Same guard, opposite curl side in every
// non-trivial case. Applies at three sites in aftertouch.hpp, not one.
inline constexpr bool kAftertouchLatchInverted = false;

// #3 — the crossbar. Reading A converges bar and post on speed >> 2 with deltaZ
// negated. The Amiga gives the bar its own treatment: it does not reflect at
// all — speed is *set* to a flat 512 and the aim point pushed 1000 out of goal.
// Set-versus-scaled is behavioural, and it is the flat bounce-out everyone
// remembers.
inline constexpr bool kCrossbarSetsSpeed = false;
inline constexpr int16_t kCrossbarSetSpeed  = 512;
inline constexpr int16_t kCrossbarPushOut   = 1000;

// #4 — the flat-3 recovery table. Reading A has it as kComputerTacklingDownTime,
// a CPU-versus-human fairness asymmetry. The Amiga has it as the *deflecting*
// tackle's recovery, for anybody. The ledger's hypothesis, worth testing
// directly: releasing fire early produces exactly the -1 sentinel the Amiga
// cannot explain, which would make early release the deflecting tackle and close
// an open question on both sides at once.
inline constexpr bool kFlat3IsDeflection = false;

// #5 — pass loft. Reading A gates a lob on the pass path. The Amiga gives passes
// no deltaZ change at all, only a one-shot +1/8 on speed.
inline constexpr bool kPassLoftEnabled = true;
inline constexpr int  kPassAftertouchSpeedShift = 3; // +1/8 when loft is off

// Ball physics constants — BALL.md §9 / B3. Amiga is the default oracle.
inline constexpr int16_t kBallGroundConstant =
    IsAmigaProfile() ? int16_t{16} : int16_t{13};
inline constexpr int16_t kBallAirConstant =
    IsAmigaProfile() ? int16_t{10} : int16_t{4};
inline constexpr int32_t kGravityConstant =
    IsAmigaProfile() ? 4608 : 3291;

} // namespace at
