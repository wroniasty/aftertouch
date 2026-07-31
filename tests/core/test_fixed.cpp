// Core unit tests. This binary links at_core and doctest and NOTHING else:
// no SDL, no ImGui. If it ever stops building on a machine with no graphics
// stack, wall 1 (PLAN.md section 0) has been breached.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "core/fixed.hpp"
#include "core/match_engine.hpp"

using at::Fix;

TEST_CASE("Fix round-trips integers") {
    CHECK(Fix::FromInt(0).ToInt() == 0);
    CHECK(Fix::FromInt(7).ToInt() == 7);
    CHECK(Fix::FromInt(-7).ToInt() == -7);
    CHECK(Fix::FromInt(32000).ToInt() == 32000);
}

TEST_CASE("Fix addition and subtraction") {
    CHECK((Fix::FromInt(3) + Fix::FromInt(4)).ToInt() == 7);
    CHECK((Fix::FromInt(10) - Fix::FromInt(4)).ToInt() == 6);
    CHECK((-Fix::FromInt(5)).ToInt() == -5);
}

TEST_CASE("Fix multiplication keeps the fractional bits") {
    // 2.5 * 4 == 10
    Fix two_point_five = Fix(Fix::kOne * 2 + Fix::kOne / 2);
    CHECK((two_point_five * Fix::FromInt(4)).ToInt() == 10);
    // 3 * 3 == 9
    CHECK((Fix::FromInt(3) * Fix::FromInt(3)).ToInt() == 9);
}

TEST_CASE("Fix division") {
    // 10 / 4 == 2.5 -> truncates to 2 on ToInt()
    Fix q = Fix::FromInt(10) / Fix::FromInt(4);
    CHECK(q.ToInt() == 2);
    CHECK(q.raw == Fix::kOne * 2 + Fix::kOne / 2);
}

TEST_CASE("Fix comparison") {
    CHECK(Fix::FromInt(1) < Fix::FromInt(2));
    CHECK(Fix::FromInt(2) == Fix::FromInt(2));
    CHECK(Fix::FromInt(3) > Fix::FromInt(-3));
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
