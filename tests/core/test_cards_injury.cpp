// B8: card and injury rolls.
#include <doctest/doctest.h>

#include "core/set_pieces.hpp"

using namespace at;

TEST_CASE("second yellow becomes sending off") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.tick = 100; // pass strictness gate sometimes — force by patching after
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].cards = 1;
    s.players[0].pos.x = Fix::FromInt(336);
    s.players[0].pos.y = Fix::FromInt(400);
    s.players[11].team_number = 2;
    s.players[11].player_ordinal = 2;
    s.players[11].pos.x = Fix::FromInt(336);
    s.players[11].pos.y = Fix::FromInt(200);
    // Fill offender mates far from goal so last-man may trigger; use ordinary.
    for (int i = 1; i < 11; ++i) {
        s.players[static_cast<size_t>(i)].team_number = 1;
        s.players[static_cast<size_t>(i)].pos.x = Fix::FromInt(336);
        s.players[static_cast<size_t>(i)].pos.y = Fix::FromInt(500);
    }
    s.resolve_rng.Seed(1);
    // Retry until a card lands (strictness + RNG).
    bool booked = false;
    for (int i = 0; i < 64; ++i) {
        s.tick = static_cast<uint32_t>(i);
        s.players[0].cards = 1;
        s.players[0].sent_away = 0;
        s.globals.which_card = 0;
        s.globals.ref_state = 0;
        const uint8_t c = RollCardForFoul(s, 0, 0, 11, false);
        if (c != 0) {
            booked = true;
            if (c == 3 || c == 2) {
                CHECK(s.players[0].cards == -1);
                CHECK(s.players[0].sent_away == 1);
            }
            CHECK(s.globals.ref_state != 0);
            break;
        }
    }
    CHECK(booked);
}

TEST_CASE("injury roll can raise injury_level") {
    MatchState s{};
    s.clock.game_length = 0;
    s.players[11].team_number = 2;
    s.players[11].injury_level = 0;
    s.resolve_rng.Seed(0xB800001u);
    bool hit = false;
    for (int i = 0; i < 200; ++i) {
        s.players[11].injury_level = 0;
        RollInjuryOnTackle(s, 1, 11);
        if (s.players[11].injury_level > 0) {
            hit = true;
            break;
        }
    }
    CHECK(hit);
}
