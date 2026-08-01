// B6: tap vs hold → quick_fire / normal_fire.
#include <doctest/doctest.h>

#include "core/match_input.hpp"
#include "core/shooting.hpp"

using namespace at;

TEST_CASE("ClassifyFireFlags: short release is quick_fire") {
    TeamControl tc{};
    ClassifyFireFlags(tc, false, 2, true);
    CHECK(tc.quick_fire == 1);
}

TEST_CASE("ClassifyFireFlags: hold reaches threshold as normal_fire") {
    TeamControl tc{};
    ClassifyFireFlags(tc, true, static_cast<int16_t>(kFireHoldThreshold - 1), true);
    CHECK(tc.normal_fire == 1);
}

TEST_CASE("ClassifyFireFlags: does not clear an existing pulse") {
    TeamControl tc{};
    tc.normal_fire = 1;
    ClassifyFireFlags(tc, true, 20, true); // past threshold, no new edge
    CHECK(tc.normal_fire == 1);
}

TEST_CASE("RefreshHumanFire: tap then release pulses quick_fire") {
    MatchState s{};
    s.sides[0].control.player_number = 1;
    MatchInput in{};
    in.p1.dir = Dir::E;
    in.p1.fire = true;
    RefreshHumanFire(s, in);
    CHECK(s.sides[0].control.fire_counter == 1);
    CHECK(s.sides[0].control.quick_fire == 0);

    in.p1.fire = false;
    RefreshHumanFire(s, in);
    CHECK(s.sides[0].control.quick_fire == 1);
    CHECK(s.sides[0].control.fire_counter == 0);
}

TEST_CASE("RefreshHumanFire: hold to threshold pulses normal_fire") {
    MatchState s{};
    s.sides[0].control.player_number = 1;
    MatchInput in{};
    in.p1.dir = Dir::N;
    in.p1.fire = true;
    for (int i = 0; i < kFireHoldThreshold - 1; ++i) {
        RefreshHumanFire(s, in);
        CHECK(s.sides[0].control.normal_fire == 0);
    }
    RefreshHumanFire(s, in);
    CHECK(s.sides[0].control.normal_fire == 1);
    CHECK(s.sides[0].control.fire_counter == kFireHoldThreshold);
}
