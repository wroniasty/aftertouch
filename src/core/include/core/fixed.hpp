#pragma once
#include <cassert>
#include <compare>   // std::strong_ordering, required by MSVC for defaulted <=>
#include <cstdint>

// 16.16 signed fixed point. This is the only numeric type the simulation uses for
// anything spatial. No float, no double, anywhere under src/core/.
//
// The feel of the original comes partly from integer truncation behaviour.
// Reimplementing in floating point produces something that looks identical and
// plays wrong, and you will not be able to find out why.
//
// The operation set is deliberately small and deliberately NOT closed: it is what
// the reference's FixedPoint offers, and no more. In particular there is no
// fixed x fixed multiply and no fixed / fixed divide, because the reference has
// neither and every rounding rule we could invent for them would be unjustified.
// Multiplication in the engine happens on raw integers in the movement kernel, as
// `trig * speed >> 8`. If a B part needs an operation that is not here, add it
// together with the reference behaviour it reproduces -- never on its own.
//
// See doc/implementation/A2-determinism-primitives.md section 2.1.

// Define AT_FIXED_CHECKED (test builds) to assert that no operation overflows.
// Release builds wrap, identically on every platform, because all raw arithmetic
// goes through uint32_t: signed overflow is undefined behaviour and a determinism
// guarantee cannot rest on UB.
#ifdef AT_FIXED_CHECKED
#define AT_FIX_ASSERT(cond) assert(cond)
#else
#define AT_FIX_ASSERT(cond) ((void)0)
#endif

namespace at {

namespace detail {

// Wrapping helpers. uint32_t arithmetic is modular by definition, and the
// uint32_t -> int32_t conversion is modular (not implementation-defined) since
// C++20, which is the standard this project builds against.
constexpr int32_t WrapAdd(int32_t a, int32_t b) {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}
constexpr int32_t WrapSub(int32_t a, int32_t b) {
    return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}
constexpr int32_t WrapNeg(int32_t a) {
    return static_cast<int32_t>(0u - static_cast<uint32_t>(a));
}
constexpr int32_t WrapShl(int32_t a, int n) {
    return static_cast<int32_t>(static_cast<uint32_t>(a) << n);
}

constexpr bool AddOverflows(int32_t a, int32_t b, int32_t r) {
    return ((a ^ r) & (b ^ r)) < 0;
}
constexpr bool SubOverflows(int32_t a, int32_t b, int32_t r) {
    return ((a ^ b) & (a ^ r)) < 0;
}

} // namespace detail

class Fix {
public:
    static constexpr int     kShift = 16;
    static constexpr int32_t kOne   = 1 << kShift;

    constexpr Fix() = default;

    // Construction is always named. There is no implicit conversion from int, so
    // `pos < 618` resolves to the mixed comparison below rather than silently
    // promoting 618 to fixed point.
    static constexpr Fix FromRaw(int32_t raw) {
        Fix f;
        f.raw_ = raw;
        return f;
    }
    static constexpr Fix FromInt(int32_t whole) {
        AT_FIX_ASSERT(whole >= -32768 && whole <= 32767);
        return FromRaw(detail::WrapShl(whole, kShift));
    }
    static constexpr Fix FromParts(int16_t whole, uint16_t frac) {
        return FromRaw(detail::WrapAdd(detail::WrapShl(whole, kShift), frac));
    }

    constexpr int32_t Raw() const { return raw_; }
    constexpr void    SetRaw(int32_t raw) { raw_ = raw; }

    // --- the three integer conversions ------------------------------------
    //
    // They disagree, and which one a call site wants is never obvious from the
    // name "ToInt", which is why that name does not exist here.

    // Arithmetic shift: FLOORS (Whole(-0.5) == -1), and narrows to 16 bits. This
    // is the "high word" read that every gameplay comparison in the reference
    // performs -- see doc/BALL.md section 1. It is the one you almost always want.
    constexpr int16_t Whole() const {
        return static_cast<int16_t>(raw_ >> kShift);
    }
    constexpr uint16_t Frac() const {
        return static_cast<uint16_t>(static_cast<uint32_t>(raw_) & 0xFFFFu);
    }
    // Toward zero (Truncated(-0.5) == 0).
    constexpr int Truncated() const {
        return Whole() + ((Sgn() < 0 && Frac() != 0) ? 1 : 0);
    }
    // Half-up, ties toward negative infinity (exactly 0.5 rounds down).
    constexpr int Rounded() const {
        return Whole() + (Frac() > 0x8000u ? 1 : 0);
    }
    // Note: zero counts as positive, as in the reference.
    constexpr int Sgn() const { return raw_ < 0 ? -1 : 1; }

    constexpr explicit operator bool() const { return raw_ != 0; }

    // --- arithmetic --------------------------------------------------------

    constexpr Fix operator+(Fix o) const {
        const int32_t r = detail::WrapAdd(raw_, o.raw_);
        AT_FIX_ASSERT(!detail::AddOverflows(raw_, o.raw_, r));
        return FromRaw(r);
    }
    constexpr Fix operator-(Fix o) const {
        const int32_t r = detail::WrapSub(raw_, o.raw_);
        AT_FIX_ASSERT(!detail::SubOverflows(raw_, o.raw_, r));
        return FromRaw(r);
    }
    constexpr Fix operator-() const {
        AT_FIX_ASSERT(raw_ != INT32_MIN);
        return FromRaw(detail::WrapNeg(raw_));
    }
    constexpr Fix operator+(int32_t whole) const { return *this + FromInt(whole); }
    constexpr Fix operator-(int32_t whole) const { return *this - FromInt(whole); }

    // Divide by a plain integer. C++ integer division truncates toward zero, so
    // this disagrees with Whole()'s flooring on negatives. That asymmetry is
    // inherited from the reference, not chosen; do not "fix" one to match the
    // other without a trace to justify it.
    constexpr Fix operator/(int32_t d) const {
        AT_FIX_ASSERT(d != 0 && !(raw_ == INT32_MIN && d == -1));
        return FromRaw(raw_ / d);
    }

    // Arithmetic right shift, well defined for negative values in C++20.
    constexpr Fix operator>>(int n) const { return FromRaw(raw_ >> n); }

    constexpr Fix& operator+=(Fix o)      { return *this = *this + o; }
    constexpr Fix& operator-=(Fix o)      { return *this = *this - o; }
    constexpr Fix& operator+=(int32_t w)  { return *this = *this + w; }
    constexpr Fix& operator-=(int32_t w)  { return *this = *this - w; }
    constexpr Fix& operator/=(int32_t d)  { return *this = *this / d; }
    constexpr Fix& operator>>=(int n)     { return *this = *this >> n; }

    // --- comparison --------------------------------------------------------

    // Fixed against fixed compares the full raw value, fraction included.
    constexpr bool operator==(const Fix&) const = default;
    constexpr auto operator<=>(const Fix&) const = default;

    // Fixed against a whole number is ASYMMETRIC, transcribed from the reference:
    //
    //     a <  n   is   a.Whole() <  n                                  (fraction ignored)
    //     a >  n   is   a.Whole() >  n || (a.Whole() == n && a.Frac())  (fraction counts)
    //     a <= n   is   !(a > n)
    //     a >= n   is   a.Whole() >= n                                  (fraction ignored)
    //
    // So 5.5 < 5 is false, 5.5 > 5 is true, and 5.5 >= 6 is false: the two
    // directions do not mirror each other. Every dead-ball barrier and goal-frame
    // test in doc/BALL.md sections 5-6 goes through these, so the asymmetry is
    // gameplay rather than sloppiness.
    constexpr bool operator<(int32_t n) const  { return Whole() < n; }
    constexpr bool operator>(int32_t n) const  { return Whole() > n || (Whole() == n && Frac() != 0); }
    constexpr bool operator<=(int32_t n) const { return !(*this > n); }
    constexpr bool operator>=(int32_t n) const { return Whole() >= n; }

private:
    int32_t raw_ = 0;
};

struct Vec2 { Fix x, y; };
struct Vec3 { Fix x, y, z; };

} // namespace at
