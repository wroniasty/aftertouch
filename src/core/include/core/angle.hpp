#pragma once
#include "core/match_input.hpp"

#include <array>
#include <cstdint>

// Two angle spaces and the octant quantiser.
// See doc/implementation/A2-determinism-primitives.md section 2.2.

namespace at {

// Game heading: 0–255, 0 = up, increasing clockwise. Wraps freely.
enum class Heading : uint8_t {};

// Table index into kSineCosineTable: 0–255, 0 = down, increasing counter-clockwise.
enum class TrigIndex : uint8_t {};

// 0–7 octant derived from a Heading.
enum class Octant : uint8_t {};

inline constexpr Heading MakeHeading(uint8_t v) { return static_cast<Heading>(v); }
inline constexpr TrigIndex MakeTrigIndex(uint8_t v) { return static_cast<TrigIndex>(v); }
inline constexpr Octant MakeOctant(uint8_t v) { return static_cast<Octant>(v); }

inline constexpr uint8_t Value(Heading h) { return static_cast<uint8_t>(h); }
inline constexpr uint8_t Value(TrigIndex t) { return static_cast<uint8_t>(t); }
inline constexpr uint8_t Value(Octant o) { return static_cast<uint8_t>(o); }

// fullDirection = (256 - angle + 128) & 0xff
inline constexpr Heading HeadingFromTrig(TrigIndex angle) {
    return MakeHeading(static_cast<uint8_t>((256 - Value(angle) + 128) & 0xff));
}

// Inverse of HeadingFromTrig: angle = (256 - heading + 128) & 0xff  (self-inverse).
inline constexpr TrigIndex TrigFromHeading(Heading h) {
    return MakeTrigIndex(static_cast<uint8_t>((256 - Value(h) + 128) & 0xff));
}

inline constexpr Octant OctantOf(Heading h) {
    return MakeOctant(static_cast<uint8_t>(((Value(h) + 16) & 0xff) >> 5));
}

// Whole-unit destination. Positions are Fix; destinations are integers
// (doc/STATE.md section 2 / BALL.md section 1).
struct Dest {
    int16_t x = 0;
    int16_t y = 0;
};

// ±1000 offsets of doc/MOVEMENT.md section 3.2. Indexed by Dir 0..7.
// One table, five consumers (movement, dribble aim, tackle, jump header, ball).
inline constexpr std::array<Dest, 8> kDefaultDestinations = {{
    {    0, -1000},  // N
    { 1000, -1000},  // NE
    { 1000,     0},  // E
    { 1000,  1000},  // SE
    {    0,  1000},  // S
    {-1000,  1000},  // SW
    {-1000,     0},  // W
    {-1000, -1000},  // NW
}};

inline constexpr Dest DestinationFor(Dir dir) {
    if (dir == Dir::None) return {};
    return kDefaultDestinations[static_cast<uint8_t>(dir)];
}

} // namespace at
