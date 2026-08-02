#pragma once
#include "core/match_clock.hpp"
#include "core/match_input.hpp"
#include "core/match_state.hpp"

#include <cstdint>

// Referee state machine — doc/implementation/B8-set-pieces.md / doc/REFEREE.md.

namespace at {

enum class RefereeState : uint8_t {
    OffScreen       = 0,
    Incoming        = 1,
    WaitingPlayer   = 2,
    AboutToGiveCard = 3,
    Booking         = 4,
    Leaving         = 5,
};

inline constexpr int16_t kRefereeSpeed     = 1024;
inline constexpr int16_t kRefHideX         = 276;
inline constexpr int16_t kRefHideY         = 439;
inline constexpr int16_t kRefWaitTicks     = 40;
inline constexpr int16_t kRefBookingTicks  = 30;

inline void ParkReferee(MatchState& s) {
    Entity& r = s.referee;
    r.pos.x = Fix::FromInt(kRefHideX);
    r.pos.y = Fix::FromInt(kRefHideY);
    r.dest_x = kRefHideX;
    r.dest_y = kRefHideY;
    r.delta = {};
    r.speed = 0;
    r.visible = 0;
    r.team_number = 3;
    s.globals.ref_state = static_cast<uint8_t>(RefereeState::OffScreen);
    s.globals.ref_timer = 0;
}

inline void ActivateReferee(MatchState& s) {
    Entity& r = s.referee;
    const int16_t dest_x = static_cast<int16_t>(s.globals.foul_x + 28);
    const int16_t dest_y = static_cast<int16_t>(s.globals.foul_y + 5);

    // Approach from "camera edge" — use pitch edge as proxy (no camera yet).
    int16_t start_y = static_cast<int16_t>(kPlayableMinY - 20);
    if (s.globals.foul_y <= kCentreSpotY)
        start_y = static_cast<int16_t>(kPlayableMaxY + 20);

    int16_t x_off = static_cast<int16_t>(s.resolve_rng.Draw() / 8);
    if (s.globals.foul_x >= kCentreSpotX) x_off = static_cast<int16_t>(-x_off);

    r.pos.x = Fix::FromInt(static_cast<int16_t>(dest_x + x_off));
    r.pos.y = Fix::FromInt(start_y);
    r.dest_x = dest_x;
    r.dest_y = dest_y;
    r.speed = kRefereeSpeed;
    r.visible = 1;
    r.team_number = 3;
    r.is_moving = 1;
    s.globals.ref_state = static_cast<uint8_t>(RefereeState::Incoming);
    s.globals.ref_timer = 0;
}

inline void UpdateReferee(MatchState& s) {
    const auto st = static_cast<RefereeState>(s.globals.ref_state);
    if (st == RefereeState::OffScreen) return;

    Entity& r = s.referee;

    if (st == RefereeState::Incoming) {
        // Step toward dest.
        const int16_t dx = static_cast<int16_t>(r.dest_x - r.pos.x.Whole());
        const int16_t dy = static_cast<int16_t>(r.dest_y - r.pos.y.Whole());
        if (dx * dx + dy * dy <= 4) {
            r.pos.x = Fix::FromInt(r.dest_x);
            r.pos.y = Fix::FromInt(r.dest_y);
            r.speed = 0;
            r.direction = static_cast<int16_t>(Dir::W); // face left
            s.globals.ref_state = static_cast<uint8_t>(RefereeState::WaitingPlayer);
            s.globals.ref_timer = kRefWaitTicks;
        } else {
            // Crude step: move up to ~speed/64 units toward dest.
            const int step = 8;
            if (dx > 0) r.pos.x += Fix::FromInt(dx > step ? step : dx);
            else if (dx < 0) r.pos.x += Fix::FromInt(dx < -step ? -step : dx);
            if (dy > 0) r.pos.y += Fix::FromInt(dy > step ? step : dy);
            else if (dy < 0) r.pos.y += Fix::FromInt(dy < -step ? -step : dy);
        }
        return;
    }

    if (st == RefereeState::WaitingPlayer) {
        if (s.globals.ref_timer > 0) --s.globals.ref_timer;
        if (s.globals.ref_timer == 0) {
            if (s.globals.which_card != 0) {
                s.globals.ref_state =
                    static_cast<uint8_t>(RefereeState::AboutToGiveCard);
            } else {
                s.globals.ref_state = static_cast<uint8_t>(RefereeState::Leaving);
                r.dest_y = (s.globals.foul_y <= kCentreSpotY) ? kPlayableMaxY
                                                              : kPlayableMinY;
                r.dest_x = r.pos.x.Whole();
                r.speed = kRefereeSpeed;
            }
        }
        return;
    }

    if (st == RefereeState::AboutToGiveCard) {
        s.globals.ref_state = static_cast<uint8_t>(RefereeState::Booking);
        s.globals.ref_timer = kRefBookingTicks;
        return;
    }

    if (st == RefereeState::Booking) {
        if (s.globals.ref_timer > 0) --s.globals.ref_timer;
        if (s.globals.ref_timer == 0) {
            // Red / 2nd yellow: send player toward left edge.
            if (s.globals.which_card >= 2 && s.globals.booked_player >= 0 &&
                s.globals.booked_player < kPitchPlayers) {
                Entity& p =
                    s.players[static_cast<size_t>(s.globals.booked_player)];
                p.dest_x = -20;
                p.dest_y = kCentreSpotY;
                p.sent_away = 1;
            }
            s.globals.ref_state = static_cast<uint8_t>(RefereeState::Leaving);
            r.dest_y = (s.globals.foul_y <= kCentreSpotY) ? kPlayableMaxY
                                                          : kPlayableMinY;
            r.dest_x = r.pos.x.Whole();
            r.speed = kRefereeSpeed;
        }
        return;
    }

    if (st == RefereeState::Leaving) {
        const int16_t dy = static_cast<int16_t>(r.dest_y - r.pos.y.Whole());
        const int step = 8;
        if (dy > step) r.pos.y += Fix::FromInt(step);
        else if (dy < -step) r.pos.y += Fix::FromInt(-step);
        else {
            ParkReferee(s);
            s.globals.which_card = 0;
            s.globals.booked_player = -1;
        }
    }
}

} // namespace at
