// B8: referee incoming → book → leave.
#include <doctest/doctest.h>

#include "core/referee.hpp"

using namespace at;

TEST_CASE("ActivateReferee enters Incoming") {
    MatchState s{};
    s.globals.foul_x = 300;
    s.globals.foul_y = 400;
    s.globals.which_card = 1;
    ActivateReferee(s);
    CHECK(s.globals.ref_state == static_cast<uint8_t>(RefereeState::Incoming));
    CHECK(s.referee.visible == 1);
    CHECK(s.referee.dest_x == 328);
    CHECK(s.referee.dest_y == 405);
}

TEST_CASE("referee advances to booking then parks") {
    MatchState s{};
    s.globals.foul_x = 300;
    s.globals.foul_y = 400;
    s.globals.which_card = 1;
    s.globals.booked_player = 0;
    ActivateReferee(s);
    // Fast-forward incoming by snapping.
    s.referee.pos.x = Fix::FromInt(s.referee.dest_x);
    s.referee.pos.y = Fix::FromInt(s.referee.dest_y);
    UpdateReferee(s);
    CHECK(s.globals.ref_state == static_cast<uint8_t>(RefereeState::WaitingPlayer));

    s.globals.ref_timer = 0;
    UpdateReferee(s);
    CHECK(s.globals.ref_state ==
          static_cast<uint8_t>(RefereeState::AboutToGiveCard));
    UpdateReferee(s);
    CHECK(s.globals.ref_state == static_cast<uint8_t>(RefereeState::Booking));

    s.globals.ref_timer = 0;
    UpdateReferee(s);
    CHECK(s.globals.ref_state == static_cast<uint8_t>(RefereeState::Leaving));

    // Snap to leave dest and finish.
    s.referee.pos.y = Fix::FromInt(s.referee.dest_y);
    UpdateReferee(s);
    CHECK(s.globals.ref_state == static_cast<uint8_t>(RefereeState::OffScreen));
    CHECK(s.referee.visible == 0);
}
