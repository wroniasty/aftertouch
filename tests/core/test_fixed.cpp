// Core unit tests. This binary links at_core and doctest and NOTHING else:
// no SDL, no ImGui. If it ever stops building on a machine with no graphics
// stack, wall 1 (PLAN.md section 0) has been breached.
//
// The Fix cases below are golden vectors for A2 work item 1. They pin rounding
// and comparison behaviour that doc/implementation/A2-determinism-primitives.md
// section 2.1 justifies; changing one of them is changing the engine's arithmetic
// and needs a trace to justify it, not a code review.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "core/fixed.hpp"
#include "core/match_engine.hpp"

using at::Fix;

namespace {
// Half a unit, the value every rounding rule disagrees about.
constexpr Fix kHalf     = Fix::FromRaw(Fix::kOne / 2);
constexpr Fix kMinusHalf = Fix::FromRaw(-(Fix::kOne / 2));
} // namespace

TEST_CASE("Fix round-trips integers") {
    CHECK(Fix::FromInt(0).Whole() == 0);
    CHECK(Fix::FromInt(7).Whole() == 7);
    CHECK(Fix::FromInt(-7).Whole() == -7);
    CHECK(Fix::FromInt(32000).Whole() == 32000);
    CHECK(Fix::FromInt(-32768).Whole() == -32768);
}

TEST_CASE("Fix addition and subtraction") {
    CHECK((Fix::FromInt(3) + Fix::FromInt(4)).Whole() == 7);
    CHECK((Fix::FromInt(10) - Fix::FromInt(4)).Whole() == 6);
    CHECK((-Fix::FromInt(5)).Whole() == -5);

    // Mixed with a plain whole number, as the reference allows.
    CHECK((Fix::FromInt(3) + 4).Whole() == 7);
    CHECK((Fix::FromInt(3) - 4).Whole() == -1);
}

// The three integer conversions disagree, which is the entire reason all three
// exist. This is the case that catches anyone reintroducing a single ToInt().
TEST_CASE("Fix conversions disagree, and each one is pinned") {
    SUBCASE("positive half") {
        CHECK(kHalf.Whole()     == 0);
        CHECK(kHalf.Truncated() == 0);
        CHECK(kHalf.Rounded()   == 0);   // ties round down
    }
    SUBCASE("negative half") {
        CHECK(kMinusHalf.Whole()     == -1);  // arithmetic shift floors
        CHECK(kMinusHalf.Truncated() ==  0);  // toward zero
        // Rounded() is Whole() + (Frac() > 0x8000), and Frac() is 0x8000 exactly,
        // so a tie rounds down in the flooring sense: -0.5 rounds to -1 while
        // +0.5 rounds to 0. All three conversions disagree on this one value.
        CHECK(kMinusHalf.Rounded()   == -1);
    }
    SUBCASE("just above half") {
        const Fix v = Fix::FromRaw(Fix::kOne / 2 + 1);
        CHECK(v.Whole()   == 0);
        CHECK(v.Rounded() == 1);
    }
    SUBCASE("negative whole with no fraction") {
        CHECK(Fix::FromInt(-3).Whole()     == -3);
        CHECK(Fix::FromInt(-3).Truncated() == -3);
        CHECK(Fix::FromInt(-3).Rounded()   == -3);
    }
}

TEST_CASE("Fix fraction is the low word, and is always positive") {
    CHECK(Fix::FromInt(5).Frac() == 0u);
    CHECK(kHalf.Frac() == 0x8000u);
    // -0.5 is stored as raw -32768, whose low word is 0x8000: the whole part
    // carries the sign and the fraction never does.
    CHECK(kMinusHalf.Frac() == 0x8000u);
    CHECK(Fix::FromParts(-1, 0x8000).Raw() == -(Fix::kOne / 2));
}

TEST_CASE("Fix division truncates toward zero, unlike Whole()") {
    CHECK((Fix::FromInt(10) / 4).Raw() == Fix::kOne * 2 + Fix::kOne / 2);
    CHECK((Fix::FromInt(10) / 4).Whole() == 2);
    // -1 raw divided by 2 truncates to 0, where a shift would floor to -1.
    CHECK((Fix::FromRaw(-1) / 2).Raw() == 0);
    CHECK((Fix::FromRaw(-1) >> 1).Raw() == -1);
}

TEST_CASE("Fix right shift is arithmetic") {
    CHECK((Fix::FromInt(8) >> 2).Whole() == 2);
    CHECK((Fix::FromInt(-8) >> 2).Whole() == -2);
    CHECK((Fix::FromRaw(-3) >> 1).Raw() == -2);   // floors, does not truncate
}

TEST_CASE("Fix compares against fixed on the full raw value") {
    CHECK(Fix::FromInt(1) < Fix::FromInt(2));
    CHECK(Fix::FromInt(2) == Fix::FromInt(2));
    CHECK(Fix::FromInt(3) > Fix::FromInt(-3));
    CHECK(Fix::FromInt(5) < (Fix::FromInt(5) + kHalf));
}

// The asymmetry is transcribed from the reference and is load-bearing at every
// boundary test in doc/BALL.md sections 5-6.
TEST_CASE("Fix compares against a whole number asymmetrically") {
    const Fix five_and_a_half = Fix::FromInt(5) + kHalf;

    CHECK_FALSE(five_and_a_half < 5);    // fraction ignored: whole is 5
    CHECK(five_and_a_half > 5);          // fraction counts
    CHECK_FALSE(five_and_a_half <= 5);
    CHECK(five_and_a_half >= 5);
    CHECK_FALSE(five_and_a_half >= 6);   // fraction ignored again

    const Fix exactly_five = Fix::FromInt(5);
    CHECK_FALSE(exactly_five > 5);
    CHECK(exactly_five <= 5);
    CHECK(exactly_five >= 5);

    // Negative side: -0.5 has Whole() == -1.
    CHECK(kMinusHalf < 0);
    CHECK(kMinusHalf > -1);
    CHECK_FALSE(kMinusHalf >= 0);
}

TEST_CASE("Fix arithmetic is constexpr and usable at compile time") {
    static_assert((Fix::FromInt(2) + Fix::FromInt(3)).Whole() == 5);
    static_assert(Fix::FromRaw(-(Fix::kOne / 2)).Whole() == -1);
    static_assert(!(Fix::FromInt(5) > 5));
    CHECK(true);
}

TEST_CASE("Fix wraps identically rather than invoking undefined behaviour") {
    // Release builds wrap; checked builds assert. Either way the value is the
    // same modular result on every platform, which is what determinism needs.
    const Fix big = Fix::FromRaw(INT32_MAX);
    CHECK(at::detail::WrapAdd(big.Raw(), 1) == INT32_MIN);
}

TEST_CASE("MatchEngine steps deterministically from a seed") {
    at::MatchEngine a, b;
    a.Reset(1234);
    b.Reset(1234);

    at::MatchInput in{};
    for (int i = 0; i < 100; ++i) {
        a.Step(in);
        b.Step(in);
    }

    CHECK(a.State().tick == 100u);
    CHECK(b.State().tick == 100u);
    CHECK(a.State().tick == b.State().tick);
}

TEST_CASE("MatchEngine::Reset clears state") {
    at::MatchEngine e;
    at::MatchInput in{};
    e.Step(in);
    e.Step(in);
    CHECK(e.State().tick == 2u);
    e.Reset(1);
    CHECK(e.State().tick == 0u);
}
