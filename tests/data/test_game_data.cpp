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

TEST_CASE("Fictional league has eight teams, three tactics, attrs in 0-15") {
    const League league = MakeFictionalLeague();
    REQUIRE(league.teams.size() == 8u);
    REQUIRE(league.tactics.size() == 3u);
    CHECK(LeagueOk(league));

    for (const auto& team : league.teams) {
        CHECK(team.players.size() == kSquadSize);
        for (const auto& p : team.players) {
            CHECK(p.attrs.passing <= 15);
            CHECK(p.attrs.shooting <= 15);
            CHECK(p.attrs.heading <= 15);
            CHECK(p.attrs.tackling <= 15);
            CHECK(p.attrs.ball_control <= 15);
            CHECK(p.attrs.speed <= 15);
            CHECK(p.attrs.finishing <= 15);
        }
    }

    // No banned marks in names.
    for (const auto& team : league.teams) {
        const std::string n(team.name.data());
        CHECK(n.find("SWOS") == std::string::npos);
        CHECK(n.find("Sensible") == std::string::npos);
    }
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
    CHECK(std::strcmp(state.teams[0].name.data(), "Northbridge FC") == 0);
    CHECK(std::strcmp(state.teams[1].name.data(), "Eastmere United") == 0);
    CHECK(state.teams[0].tactics_id == 0);
    CHECK(state.teams[1].tactics_id == 1);

    // First eleven of each side.
    CHECK(state.shirt_numbers[0] == 1);    // home keeper
    CHECK(state.positions[0] == static_cast<uint8_t>(Position::GK));
    CHECK(state.shirt_numbers[11] == 1);   // away keeper
    CHECK(state.positions[11] == static_cast<uint8_t>(Position::GK));

    // Strength-4 Northbridge vs strength-5 Eastmere: away striker finishing high.
    CHECK(state.player_attrs[9].finishing <= 15);
    CHECK(state.player_attrs[20].finishing <= 15);
    CHECK(state.player_attrs[20].finishing >= state.player_attrs[9].finishing);

    for (const auto& a : state.player_attrs) {
        CHECK(a.passing <= 15);
        CHECK(a.shooting <= 15);
        CHECK(a.heading <= 15);
        CHECK(a.tackling <= 15);
        CHECK(a.ball_control <= 15);
        CHECK(a.speed <= 15);
        CHECK(a.finishing <= 15);
    }

    // Kits projected.
    CHECK(state.teams[0].primary.shirt == 1);
    CHECK(state.teams[1].primary.shirt == 6);
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
    team.players[0].attrs.passing = 0x0A;
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
