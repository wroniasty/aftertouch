#pragma once
#include "data/game_data.hpp"

// The committed-in-code default dataset. A fresh clone has a playable league with
// no original install and no generator step — same rule A4 applies to placeholder
// art. Names are invented; nothing here is a real club or a Sensible mark.

namespace at::data {

inline void SetName(std::array<char, kNameLen>& dst, const char* src) {
    dst.fill('\0');
    for (size_t i = 0; i < kNameLen - 1 && src[i]; ++i) dst[i] = src[i];
}

inline void SetTacticName(std::array<char, kTacticNameLen>& dst, const char* src) {
    dst.fill('\0');
    for (size_t i = 0; i < kTacticNameLen - 1 && src[i]; ++i) dst[i] = src[i];
}

namespace fictional_detail {

// Pack a destination on the 16×15 authoring grid (x 0..14, y 0..15).
inline constexpr uint8_t Cell(uint8_t x, uint8_t y) {
    return static_cast<uint8_t>(((x & 0x0Fu) << 4) | (y & 0x0Fu));
}

// Crude formation shapes authored as "we defend the top goal".
// Engine ball grid is 5 cols × 7 rows (q = row*5+col). Role y stays in the
// defending half at centre-ball; pushes deeper as the ball advances.
// shape: 0 = 4-4-2, 1 = 5-3-2, 2 = 3-5-2
inline TacticRecord MakeTactic(const char* name, uint8_t out_of_play,
                               int shape) {
    // Per-role (x_slot 0..14, y_base 0..15) for centre column.
    // Defenders ~2–3, mids ~4–5, attackers ~6–7 → own half at ball_row≈3.
    struct RoleXY {
        uint8_t x;
        uint8_t y;
    };
    RoleXY roles[10]{};
    if (shape == 1) {
        // 5-3-2
        const RoleXY src[10] = {
            {1, 2}, {4, 2}, {7, 3}, {10, 2}, {13, 2}, // five backs
            {3, 5}, {7, 5}, {11, 5},                  // three mids
            {5, 7}, {9, 7},                           // two attackers
        };
        for (int i = 0; i < 10; ++i) roles[i] = src[i];
    } else if (shape == 2) {
        // 3-5-2
        const RoleXY src[10] = {
            {3, 2}, {7, 3}, {11, 2},                  // three backs
            {1, 4}, {4, 5}, {7, 5}, {10, 5}, {13, 4}, // five mids
            {5, 7}, {9, 7},                           // two attackers
        };
        for (int i = 0; i < 10; ++i) roles[i] = src[i];
    } else {
        // 4-4-2
        const RoleXY src[10] = {
            {2, 2}, {5, 3}, {9, 3}, {12, 2}, // four backs
            {2, 5}, {5, 5}, {9, 5}, {12, 5}, // four mids
            {5, 7}, {9, 7},                  // two attackers
        };
        for (int i = 0; i < 10; ++i) roles[i] = src[i];
    }

    TacticRecord t{};
    SetTacticName(t.name, name);
    t.out_of_play = out_of_play;
    for (size_t r = 0; r < kTacticRoles; ++r) {
        for (size_t q = 0; q < kBallQuadrants; ++q) {
            const int col = static_cast<int>(q % 5);
            const int row = static_cast<int>(q / 5);
            int x = static_cast<int>(roles[r].x) + (col - 2) * 2;
            if (x < 0) x = 0;
            if (x > 14) x = 14;
            // Push with ball row; keep centre-ball (row~3) in own half (y<=7).
            int y = static_cast<int>(roles[r].y);
            if (row > 3) y += (row - 3);
            if (y < 0) y = 0;
            if (y > 15) y = 15;
            t.cells[r][q] = Cell(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        }
    }
    return t;
}

inline PlayerRecord MakePlayer(const char* name, uint8_t shirt, Position pos,
                               uint8_t pass, uint8_t shoot, uint8_t head,
                               uint8_t tackle, uint8_t control, uint8_t speed,
                               uint8_t finish, Face face = Face::White) {
    PlayerRecord p{};
    SetName(p.name, name);
    p.shirt    = shirt;
    p.position = static_cast<uint8_t>(pos);
    p.face     = static_cast<uint8_t>(face);
    p.attrs.passing      = pass;
    p.attrs.shooting     = shoot;
    p.attrs.heading      = head;
    p.attrs.tackling     = tackle;
    p.attrs.ball_control = control;
    p.attrs.speed        = speed;
    p.attrs.finishing    = finish;
    p.price_index        = static_cast<uint8_t>((pass + shoot + speed) / 3);
    return p;
}

inline void FillSquad(TeamRecord& team, uint8_t strength) {
    // strength 0–5 shifts the whole squad within 0–15 without leaving the range.
    const auto bump = [strength](uint8_t base) -> uint8_t {
        const int v = static_cast<int>(base) + static_cast<int>(strength);
        return static_cast<uint8_t>(v > 15 ? 15 : v);
    };

    team.players[0]  = MakePlayer("Keeper",   1, Position::GK, bump(6), bump(2), bump(8),
                                  bump(4), bump(7), bump(5), bump(2));
    team.players[1]  = MakePlayer("Rightback",2, Position::RB, bump(7), bump(3), bump(7),
                                  bump(9), bump(7), bump(8), bump(3), Face::Ginger);
    team.players[2]  = MakePlayer("Leftback", 3, Position::LB, bump(7), bump(3), bump(7),
                                  bump(9), bump(7), bump(8), bump(3), Face::Black);
    team.players[3]  = MakePlayer("Centre A", 4, Position::D,  bump(6), bump(2), bump(10),
                                  bump(11), bump(6), bump(6), bump(2));
    team.players[4]  = MakePlayer("Centre B", 5, Position::D,  bump(6), bump(2), bump(10),
                                  bump(11), bump(6), bump(6), bump(2));
    team.players[5]  = MakePlayer("Right mid",6, Position::RW, bump(9), bump(6), bump(6),
                                  bump(6), bump(9), bump(9), bump(6), Face::Ginger);
    team.players[6]  = MakePlayer("Left mid", 7, Position::LW, bump(9), bump(6), bump(6),
                                  bump(6), bump(9), bump(9), bump(6), Face::Black);
    team.players[7]  = MakePlayer("Central",  8, Position::M,  bump(10), bump(7), bump(7),
                                  bump(7), bump(10), bump(8), bump(7));
    team.players[8]  = MakePlayer("Playmaker",9, Position::M,  bump(11), bump(8), bump(6),
                                  bump(5), bump(11), bump(7), bump(8));
    team.players[9]  = MakePlayer("Striker", 10, Position::A,  bump(7), bump(11), bump(9),
                                  bump(4), bump(9), bump(9), bump(12));
    team.players[10] = MakePlayer("Poacher", 11, Position::A,  bump(6), bump(12), bump(8),
                                  bump(3), bump(8), bump(10), bump(13), Face::Ginger);
    for (size_t i = 11; i < kSquadSize; ++i) {
        char buf[16] = "Sub ";
        buf[4] = static_cast<char>('0' + static_cast<int>(i - 10));
        team.players[i] = MakePlayer(buf, static_cast<uint8_t>(12 + (i - 11)),
                                     Position::M, bump(6), bump(5), bump(6),
                                     bump(6), bump(7), bump(7), bump(5));
    }
}

inline TeamRecord MakeTeam(uint16_t id, const char* name, const char* coach,
                           uint8_t tactics_id, uint8_t strength,
                           KitRecord primary, KitRecord secondary) {
    TeamRecord t{};
    t.id = id;
    SetName(t.name, name);
    SetName(t.coach, coach);
    t.tactics_id = tactics_id;
    t.primary = primary;
    t.secondary = secondary;
    FillSquad(t, strength);
    return t;
}

} // namespace fictional_detail

inline League MakeFictionalLeague() {
    using namespace fictional_detail;
    League league;
    league.tactics.push_back(MakeTactic("4-4-2", 0, 0));
    league.tactics.push_back(MakeTactic("5-3-2", 1, 1));
    league.tactics.push_back(MakeTactic("3-5-2", 2, 2));

    const KitRecord red_white{static_cast<uint8_t>(ShirtType::Plain), 0, 1, 9, 1};
    const KitRecord blue_white{static_cast<uint8_t>(ShirtType::VerticalStripes), 2, 2, 9, 2};
    const KitRecord green{static_cast<uint8_t>(ShirtType::Plain), 0, 6, 6, 6};
    const KitRecord amber{static_cast<uint8_t>(ShirtType::ColouredSleeves), 3, 3, 0, 3};
    const KitRecord black_red{static_cast<uint8_t>(ShirtType::HorizontalStripes), 1, 0, 1, 0};
    const KitRecord purple{static_cast<uint8_t>(ShirtType::Plain), 0, 8, 9, 8};
    const KitRecord teal{static_cast<uint8_t>(ShirtType::VerticalStripes), 6, 6, 9, 2};
    const KitRecord gold{static_cast<uint8_t>(ShirtType::Plain), 0, 3, 0, 3};

    league.teams.push_back(MakeTeam(1, "Northbridge FC",   "A. Hale",    0, 4, red_white, blue_white));
    league.teams.push_back(MakeTeam(2, "Port Meridian",    "J. Crowe",   0, 3, blue_white, red_white));
    league.teams.push_back(MakeTeam(3, "Eastmere United",  "S. Quinn",   1, 5, green, amber));
    league.teams.push_back(MakeTeam(4, "Hollowford",       "M. Voss",    1, 2, amber, green));
    league.teams.push_back(MakeTeam(5, "Riverside Athletic","L. Pike",   2, 3, black_red, gold));
    league.teams.push_back(MakeTeam(6, "Castlewick Town",  "R. Dunn",    0, 1, purple, teal));
    league.teams.push_back(MakeTeam(7, "Southport Rovers", "K. Ames",    2, 4, teal, purple));
    league.teams.push_back(MakeTeam(8, "Highcliff",        "P. Marsh",   1, 2, gold, black_red));
    return league;
}

} // namespace at::data
