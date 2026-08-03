// A5: game data format, fictional league, kickoff projection.
// See doc/implementation/A5-game-data.md section 5.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstring>
#include <string>
#include <vector>

#include "data/fictional.hpp"
#include "data/game_data.hpp"

using namespace at;
using namespace at::data;

namespace {

bool SamePlayer(const PlayerRecord& a, const PlayerRecord& b) {
    return std::memcmp(a.name.data(), b.name.data(), kNameLen) == 0 &&
           a.nationality == b.nationality && a.shirt == b.shirt &&
           a.position == b.position && a.face == b.face &&
           a.attrs.passing == b.attrs.passing &&
           a.attrs.shooting == b.attrs.shooting &&
           a.attrs.heading == b.attrs.heading &&
           a.attrs.tackling == b.attrs.tackling &&
           a.attrs.ball_control == b.attrs.ball_control &&
           a.attrs.speed == b.attrs.speed &&
           a.attrs.finishing == b.attrs.finishing &&
           a.price_index == b.price_index;
}

bool SameKit(const KitRecord& a, const KitRecord& b) {
    return a.shirt_type == b.shirt_type && a.stripes == b.stripes &&
           a.shirt == b.shirt && a.shorts == b.shorts && a.socks == b.socks;
}

bool SameLeague(const League& a, const League& b) {
    if (a.tactics.size() != b.tactics.size()) return false;
    if (a.teams.size() != b.teams.size()) return false;
    for (size_t i = 0; i < a.tactics.size(); ++i) {
        const auto& ta = a.tactics[i];
        const auto& tb = b.tactics[i];
        if (std::memcmp(ta.name.data(), tb.name.data(), kTacticNameLen) != 0) return false;
        if (ta.out_of_play != tb.out_of_play) return false;
        if (ta.cells != tb.cells) return false;
    }
    for (size_t i = 0; i < a.teams.size(); ++i) {
        const auto& ta = a.teams[i];
        const auto& tb = b.teams[i];
        if (ta.id != tb.id) return false;
        if (std::memcmp(ta.name.data(), tb.name.data(), kNameLen) != 0) return false;
        if (std::memcmp(ta.coach.data(), tb.coach.data(), kNameLen) != 0) return false;
        if (ta.tactics_id != tb.tactics_id || ta.tier != tb.tier) return false;
        if (!SameKit(ta.primary, tb.primary) || !SameKit(ta.secondary, tb.secondary))
            return false;
        for (size_t p = 0; p < kSquadSize; ++p)
            if (!SamePlayer(ta.players[p], tb.players[p])) return false;
    }
    return true;
}

} // namespace

TEST_CASE("Fictional league has eight teams, three tactics, attrs in 0-7") {
    const League league = MakeFictionalLeague();
    REQUIRE(league.teams.size() == 8u);
    REQUIRE(league.tactics.size() == 3u);
    CHECK(LeagueOk(league));

    for (const auto& team : league.teams) {
        CHECK(team.players.size() == kSquadSize);
        for (const auto& p : team.players) {
            CHECK(p.attrs.passing <= kAttrMax);
            CHECK(p.attrs.shooting <= kAttrMax);
            CHECK(p.attrs.heading <= kAttrMax);
            CHECK(p.attrs.tackling <= kAttrMax);
            CHECK(p.attrs.ball_control <= kAttrMax);
            CHECK(p.attrs.speed <= kAttrMax);
            CHECK(p.attrs.finishing <= kAttrMax);
        }
    }

    // No banned marks in names.
    for (const auto& team : league.teams) {
        const std::string n(team.name.data());
        CHECK(n.find("SWOS") == std::string::npos);
        CHECK(n.find("Sensible") == std::string::npos);
    }
}

// B13 / R2. The 0–15 → 0–7 change is only safe if the design canvas is
// *projected* rather than clamped. Clamping would have pushed every strong team
// to the cap and erased both axes of variation the squad table exists to
// express, and it would have done so silently — every other test here would
// still pass. These three checks are what make the projection falsifiable.
TEST_CASE("the 0-7 projection preserves team and positional variation") {
    const League league = MakeFictionalLeague();

    const auto squad_total = [](const TeamRecord& t) {
        int sum = 0;
        for (const auto& p : t.players)
            sum += p.attrs.passing + p.attrs.shooting + p.attrs.heading +
                   p.attrs.tackling + p.attrs.ball_control + p.attrs.speed +
                   p.attrs.finishing;
        return sum;
    };

    // Team strength survives quantisation: Eastmere (strength 5) is built
    // stronger than Castlewick (strength 1) and must still read that way.
    const TeamRecord* strong = nullptr;
    const TeamRecord* weak   = nullptr;
    for (const auto& t : league.teams) {
        if (t.id == 3) strong = &t;
        if (t.id == 6) weak = &t;
    }
    REQUIRE(strong);
    REQUIRE(weak);
    CHECK(squad_total(*strong) > squad_total(*weak));

    // Positional shape survives: a poacher finishes better than a centre-half.
    for (const auto& t : league.teams) {
        CHECK(t.players[10].attrs.finishing > t.players[3].attrs.finishing);
        CHECK(t.players[3].attrs.tackling > t.players[10].attrs.tackling);
    }

    // The range is used, not saturated. If projection had degenerated into a
    // clamp, the strongest squad would be a wall of 7s.
    int at_cap = 0, total = 0;
    for (const auto& p : strong->players) {
        for (uint8_t v : {p.attrs.passing, p.attrs.shooting, p.attrs.heading,
                          p.attrs.tackling, p.attrs.ball_control, p.attrs.speed,
                          p.attrs.finishing}) {
            if (v == kAttrMax) ++at_cap;
            ++total;
        }
    }
    CHECK(at_cap * 2 < total); // fewer than half the strongest squad's attrs max out
}

TEST_CASE("League round-trips through ATGD") {
    const League league = MakeFictionalLeague();
    std::vector<uint8_t> buf(LeagueByteSize(league));
    const size_t n = EncodeLeague(league, buf);
    REQUIRE(n == buf.size());
    REQUIRE(n > kHeaderSize);

    League out;
    REQUIRE(DecodeLeague(buf, out));
    CHECK(SameLeague(league, out));
    CHECK(ValidateLeagueBytes(buf));
}

TEST_CASE("Malformed league bytes are rejected") {
    const League league = MakeFictionalLeague();
    std::vector<uint8_t> buf(LeagueByteSize(league));
    REQUIRE(EncodeLeague(league, buf) == buf.size());

    SUBCASE("wrong magic") {
        auto bad = buf;
        bad[0] ^= 0xFF;
        League out;
        CHECK_FALSE(DecodeLeague(bad, out));
    }
    SUBCASE("unknown version") {
        auto bad = buf;
        bad[4] = 0x7F;
        League out;
        CHECK_FALSE(DecodeLeague(bad, out));
    }
    SUBCASE("truncated") {
        auto bad = buf;
        bad.resize(bad.size() / 2);
        League out;
        CHECK_FALSE(DecodeLeague(bad, out));
    }
    SUBCASE("tampered body fails fingerprint") {
        auto bad = buf;
        bad[kHeaderSize + 10] ^= 0x01;
        League out;
        CHECK_FALSE(DecodeLeague(bad, out));
    }
    SUBCASE("empty") {
        League out;
        CHECK_FALSE(DecodeLeague(std::vector<uint8_t>{}, out));
    }
}

TEST_CASE("Attribute above 15 is rejected by the encoder") {
    League league = MakeFictionalLeague();
    league.teams[0].players[0].attrs.speed = 16;
    CHECK_FALSE(LeagueOk(league));
    std::vector<uint8_t> buf(LeagueByteSize(league) + 64);
    CHECK(EncodeLeague(league, buf) == 0u);
}

TEST_CASE("Career snapshot round-trips and references teams by id") {
    CareerSnapshot c;
    c.season_year = 1996;
    c.game_type   = CareerGameType::Season;
    c.balance     = 1'250'000;
    c.old_balance = 1'000'000;
    c.team_ids    = {1, 2, 3, 4, 5, 6, 7, 8};

    std::vector<uint8_t> buf(CareerByteSize(c));
    REQUIRE(EncodeCareer(c, buf) == buf.size());

    CareerSnapshot out;
    REQUIRE(DecodeCareer(buf, out));
    CHECK(out.season_year == c.season_year);
    CHECK(out.game_type == c.game_type);
    CHECK(out.balance == c.balance);
    CHECK(out.old_balance == c.old_balance);
    CHECK(out.team_ids == c.team_ids);
}

TEST_CASE("ApplyKickoff loads a fictional league into MatchState") {
    const League league = MakeFictionalLeague();
    MatchState state;
    REQUIRE(ApplyKickoff(league, 1, 3, state));

    CHECK(state.phase == MatchPhase::KickOff);
    CHECK(std::strcmp(state.sides[0].sheet.name.data(), "Northbridge FC") == 0);
    CHECK(std::strcmp(state.sides[1].sheet.name.data(), "Eastmere United") == 0);
    CHECK(state.sides[0].sheet.tactics_id == 0);
    CHECK(state.sides[1].sheet.tactics_id == 1);

    // First eleven of each side (squad + pitch identity).
    CHECK(state.sides[0].squad[0].shirt_number == 1);
    CHECK(state.sides[0].squad[0].position == static_cast<uint8_t>(Position::GK));
    CHECK(state.sides[1].squad[0].shirt_number == 1);
    CHECK(state.sides[1].squad[0].position == static_cast<uint8_t>(Position::GK));
    CHECK(state.players[0].team_number == 1);
    CHECK(state.players[0].player_ordinal == 1);
    CHECK(state.players[11].team_number == 2);
    CHECK(state.referee.team_number == 3);

    // All 16 squad slots filled; tactics snapshot present.
    CHECK(state.sides[0].squad[11].shirt_number != 0);
    CHECK(state.sides[0].squad[15].index == 15);
    CHECK(state.sides[0].tactics.cells ==
          league.tactics[state.sides[0].sheet.tactics_id].cells);

    // Strength-4 Northbridge vs strength-5 Eastmere: away striker finishing high.
    // Pitch slot 9 = home #10 (index 9); slot 20 = away #10 (side1 squad index 9).
    CHECK(state.sides[0].squad[9].attrs.finishing <= 15);
    CHECK(state.sides[1].squad[9].attrs.finishing <= 15);
    CHECK(state.sides[1].squad[9].attrs.finishing >=
          state.sides[0].squad[9].attrs.finishing);

    for (const auto& side : state.sides) {
        for (const auto& sp : side.squad) {
            CHECK(sp.attrs.passing <= 15);
            CHECK(sp.attrs.shooting <= 15);
            CHECK(sp.attrs.heading <= 15);
            CHECK(sp.attrs.tackling <= 15);
            CHECK(sp.attrs.ball_control <= 15);
            CHECK(sp.attrs.speed <= 15);
            CHECK(sp.attrs.finishing <= 15);
        }
    }

    CHECK(state.sides[0].sheet.primary.shirt == 1);
    CHECK(state.sides[1].sheet.primary.shirt == 6);
}

TEST_CASE("ApplyKickoff rejects unknown or identical teams") {
    const League league = MakeFictionalLeague();
    MatchState state;
    CHECK_FALSE(ApplyKickoff(league, 1, 1, state));
    CHECK_FALSE(ApplyKickoff(league, 1, 99, state));
    CHECK_FALSE(ApplyKickoff(league, 99, 1, state));
}

TEST_CASE("ATGD encoding is explicitly little-endian") {
    League league;
    TacticRecord tac{};
    SetTacticName(tac.name, "T");
    league.tactics.push_back(tac);

    TeamRecord team{};
    team.id = 0x0201;
    SetName(team.name, "A");
    SetName(team.coach, "B");
    team.players[0].attrs.passing = 0x06; // was 0x0A — AttrsValid now caps at 7 (B13 / R2)
    league.teams.push_back(team);

    std::vector<uint8_t> buf(LeagueByteSize(league));
    REQUIRE(EncodeLeague(league, buf) == buf.size());

    // magic "ATGD"
    CHECK(buf[0] == 'A');
    CHECK(buf[1] == 'T');
    CHECK(buf[2] == 'G');
    CHECK(buf[3] == 'D');

    // team id at teams section: little-endian 0x0201
    const uint32_t teams_off = static_cast<uint32_t>(kHeaderSize + kTacticWireSize);
    CHECK(buf[teams_off + 0] == 0x01);
    CHECK(buf[teams_off + 1] == 0x02);
}

// The AI team slid sideways with the ball but never changed depth. Cause: the
// tactics grid pushed the shape only when the ball row was past halfway, so the
// four rows from the far byline to just past the centre all produced an
// identical formation — over half the pitch in which the ball moved and the
// team's y did not. Columns had no such dead zone, which is exactly the
// asymmetry the C1A trace shows.
TEST_CASE("the tactics grid responds to the ball row across the whole pitch") {
    const League league = MakeFictionalLeague();
    REQUIRE(!league.tactics.empty());

    for (const auto& tac : league.tactics) {
        // A midfield role: far enough from the clamps to move at every row.
        constexpr size_t kMid = 5;
        std::vector<int> depths;
        for (int row = 0; row < 7; ++row) {
            const uint8_t cell = tac.cells[kMid][static_cast<size_t>(row * 5 + 2)];
            depths.push_back(cell & 0x0F);
        }
        // Monotonic in the ball row, and it actually spans a useful range.
        for (size_t i = 1; i < depths.size(); ++i)
            CHECK(depths[i] >= depths[i - 1]);
        CHECK(depths.back() > depths.front());
        CHECK(depths.back() - depths.front() >= 4);

        // No four consecutive rows share a depth — that dead zone was the bug.
        for (size_t i = 3; i < depths.size(); ++i) {
            const bool flat = depths[i] == depths[i - 1] &&
                              depths[i - 1] == depths[i - 2] &&
                              depths[i - 2] == depths[i - 3];
            CHECK_FALSE(flat);
        }
    }
}

TEST_CASE("no tactics cell puts a player on his own goal line") {
    const League league = MakeFictionalLeague();
    for (const auto& tac : league.tactics)
        for (size_t r = 0; r < kTacticRoles; ++r)
            for (size_t q = 0; q < kBallQuadrants; ++q) {
                const uint8_t y = tac.cells[r][q] & 0x0F;
                CHECK(y >= 1);
                CHECK(y <= 14);
            }
}
