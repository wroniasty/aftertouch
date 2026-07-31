#pragma once
#include <compare>   // std::strong_ordering, required by MSVC for defaulted <=>
#include <cstdint>

namespace at {

// 16.16 signed fixed point. This is the only numeric type the simulation uses
// for anything spatial. No float, no double, anywhere under src/core/.
//
// The feel of the original comes partly from integer truncation behaviour.
// Reimplementing in floating point produces something that looks identical and
// plays wrong, and you will not be able to find out why.
struct Fix {
    int32_t raw = 0;

    static constexpr int kShift = 16;
    static constexpr int32_t kOne = 1 << kShift;

    constexpr Fix() = default;
    constexpr explicit Fix(int32_t r) : raw(r) {}

    static constexpr Fix FromInt(int32_t v) { return Fix(v << kShift); }
    constexpr int32_t ToInt() const { return raw >> kShift; }

    constexpr Fix operator+(Fix o) const { return Fix(raw + o.raw); }
    constexpr Fix operator-(Fix o) const { return Fix(raw - o.raw); }
    constexpr Fix operator-() const      { return Fix(-raw); }

    // NOTE: rounding semantics here are PLACEHOLDERS. When trace-diffing against
    // a reference implementation begins (PLAN.md section 9, Phase 0), matching
    // the exact truncation/rounding of multiply and divide is among the first
    // things that must be pinned down. Do not "fix" these casually.
    constexpr Fix operator*(Fix o) const {
        return Fix(static_cast<int32_t>(
            (static_cast<int64_t>(raw) * o.raw) >> kShift));
    }
    constexpr Fix operator/(Fix o) const {
        return Fix(static_cast<int32_t>(
            (static_cast<int64_t>(raw) << kShift) / o.raw));
    }

    constexpr bool operator==(const Fix&) const = default;
    constexpr auto operator<=>(const Fix&) const = default;
};

struct Vec2 { Fix x, y; };
struct Vec3 { Fix x, y, z; };

} // namespace at
