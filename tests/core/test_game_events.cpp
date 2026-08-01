// B10: pure GameControlEvents → Dir.
#include <doctest/doctest.h>

#include "core/game_events.hpp"

using namespace at;

TEST_CASE("EventsToDir cardinals and none") {
    CHECK(EventsToDir(0) == Dir::None);
    CHECK(EventsToDir(kGameEventUp) == Dir::N);
    CHECK(EventsToDir(kGameEventRight) == Dir::E);
    CHECK(EventsToDir(kGameEventDown) == Dir::S);
    CHECK(EventsToDir(kGameEventLeft) == Dir::W);
}

TEST_CASE("EventsToDir diagonals beat cardinals") {
    CHECK(EventsToDir(kGameEventUp | kGameEventRight) == Dir::NE);
    CHECK(EventsToDir(kGameEventDown | kGameEventLeft) == Dir::SW);
    CHECK(EventsToDir(kGameEventUp | kGameEventLeft) == Dir::NW);
    CHECK(EventsToDir(kGameEventDown | kGameEventRight) == Dir::SE);
}

TEST_CASE("FilterOverlappedEvents keeps previous axis") {
    const uint32_t both_v = kGameEventUp | kGameEventDown;
    CHECK((FilterOverlappedEvents(both_v, kGameEventUp) & kGameEventUp) == 0);
    CHECK((FilterOverlappedEvents(both_v, kGameEventUp) & kGameEventDown) != 0);
    CHECK((FilterOverlappedEvents(both_v, kGameEventDown) & kGameEventDown) == 0);
    CHECK((FilterOverlappedEvents(both_v, kGameEventDown) & kGameEventUp) != 0);

    const uint32_t both_h = kGameEventLeft | kGameEventRight;
    CHECK((FilterOverlappedEvents(both_h, kGameEventLeft) & kGameEventLeft) == 0);
    CHECK((FilterOverlappedEvents(both_h, 0) & kGameEventLeft) != 0); // default keep left
}

TEST_CASE("EventsToPlayerInput sets fire from Kick") {
    auto p = EventsToPlayerInput(kGameEventRight | kGameEventKick);
    CHECK(p.dir == Dir::E);
    CHECK(p.fire);
    p = EventsToPlayerInput(kGameEventUp);
    CHECK_FALSE(p.fire);
}
