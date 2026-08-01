// A3 work item 1 / B1 ATTR v2: the trace record format.
#include <doctest/doctest.h>

#include <array>
#include <cstring>
#include <span>
#include <vector>

#include "core/trace.hpp"

using namespace at;

namespace {

MatchState MakeBusyState() {
    MatchState s{};
    s.tick      = 0x01020304u;
    s.phase     = MatchPhase::Goal;
    s.last_roll = 0xAB;
    s.score     = {3, 5};

    s.ball.pos   = {Fix::FromRaw(11), Fix::FromRaw(-22), Fix::FromRaw(33)};
    s.ball.delta = {Fix::FromRaw(-44), Fix::FromRaw(55), Fix::FromRaw(-66)};
    s.ball.speed = 7;
    s.ball.full_direction = 9;
    s.ball.player_state = static_cast<uint8_t>(PlayerState::Happy);

    for (size_t i = 0; i < s.players.size(); ++i) {
        const int32_t n = static_cast<int32_t>(i) + 1;
        s.players[i].pos = {Fix::FromRaw(n * 100), Fix::FromRaw(-n * 200),
                            Fix::FromRaw(n * 300)};
        s.players[i].delta = {Fix::FromRaw(-n * 400), Fix::FromRaw(n * 500),
                              Fix::FromRaw(-n * 600)};
        s.players[i].frame_index = static_cast<int16_t>(n);
        s.players[i].cards      = static_cast<int16_t>(255 - n);
        s.players[i].team_number =
            static_cast<int16_t>((i < 11) ? 1 : 2);
        s.players[i].player_ordinal = static_cast<int16_t>((i % 11) + 1);
    }

    s.referee.team_number = 3;
    s.booked_indicator.image_index = 42;

    s.sides[0].sheet.tactics_id = 1;
    s.sides[0].sheet.name[0] = 'A';
    s.sides[0].control.spin_timer = 12;
    s.sides[0].control.quick_fire = 1;
    s.sides[0].squad[0].attrs.finishing = 11;
    s.sides[0].squad[0].full_name[0] = 'Z';
    s.sides[0].tactics.out_of_play = 3;
    s.sides[0].tactics.cells[0][0] = 0xAB;
    s.sides[1].control.controlled_slot = 15;
    s.sides[1].squad[15].shirt_number = 99;

    s.globals.game_state = 7;
    s.globals.foul_x = -3;
    s.globals.ball_next_y = 100;

    s.clock.game_length = 0;
    s.clock.period = 1;
    s.clock.displayed_minute = 67;
    s.clock.game_seconds = 12;
    s.clock.match_started = 1;
    s.sides[0].stats.possession = 1234;
    s.sides[1].stats.corners_won = 5;

    s.gameplay_rng.Seed(1);
    s.presentation_rng.Seed(2);
    s.resolve_rng.Seed(3);
    return s;
}

MatchInput MakeBusyInput() {
    MatchInput in{};
    in.p1.dir  = Dir::SE;
    in.p1.fire = true;
    in.p2.dir  = Dir::NW;
    in.p2.fire = false;
    return in;
}

} // namespace

TEST_CASE("Trace record round-trips a fully populated state") {
    const MatchState s  = MakeBusyState();
    const MatchInput in = MakeBusyInput();

    std::array<uint8_t, trace::kRecordSize> buf{};
    CHECK(trace::SerializeRecord(s, in, buf) == trace::kRecordSize);

    MatchState out{};
    MatchInput out_in{};
    REQUIRE(trace::DeserializeRecord(buf, out, out_in));

    CHECK(std::memcmp(&s, &out, sizeof(MatchState)) == 0);
    CHECK(out_in.p1.dir == in.p1.dir);
    CHECK(out_in.p1.fire == in.p1.fire);
    CHECK(out_in.p2.dir == in.p2.dir);
    CHECK(out_in.p2.fire == in.p2.fire);
}

TEST_CASE("Trace record is fixed width, so tick maps to offset by arithmetic") {
    const MatchState s  = MakeBusyState();
    const MatchInput in = MakeBusyInput();

    std::array<uint8_t, trace::kRecordSize> a{}, b{};
    MatchState empty{};
    CHECK(trace::SerializeRecord(s, in, a) == trace::SerializeRecord(empty, in, b));
    CHECK(trace::kRecordSize ==
          trace::kRecordPrefixSize +
              trace::kArenaEntityCount * trace::kEntityWireSize +
              2 * trace::kSideWireSize + trace::kGlobalsWireSize +
              trace::kClockWireSize + 12 + 8);
}

TEST_CASE("Trace record refuses a buffer that is too small") {
    std::array<uint8_t, trace::kRecordSize - 1> small{};
    CHECK(trace::SerializeRecord(MakeBusyState(), MakeBusyInput(), small) == 0);

    MatchState s{};
    MatchInput in{};
    CHECK_FALSE(trace::DeserializeRecord(small, s, in));
}

TEST_CASE("Trace record detects corruption anywhere in the payload") {
    std::array<uint8_t, trace::kRecordSize> buf{};
    trace::SerializeRecord(MakeBusyState(), MakeBusyInput(), buf);

    for (size_t i : {size_t{0}, size_t{5}, size_t{100},
                     trace::kRecordSize - 9}) {
        auto corrupt = buf;
        corrupt[i] ^= 0x01;
        MatchState s{};
        MatchInput in{};
        CAPTURE(i);
        CHECK_FALSE(trace::DeserializeRecord(corrupt, s, in));
    }
}

TEST_CASE("Trace record hash is readable without decoding the record") {
    std::array<uint8_t, trace::kRecordSize> buf{};
    trace::SerializeRecord(MakeBusyState(), MakeBusyInput(), buf);

    const uint64_t scanned = trace::RecordHash(buf);
    const uint64_t computed =
        trace::HashBytes(std::span<const uint8_t>(buf).subspan(0, trace::kRecordSize - 8));
    CHECK(scanned == computed);
}

TEST_CASE("Differing states produce differing record hashes") {
    MatchState a = MakeBusyState();
    MatchState b = a;
    b.players[17].pos.x = Fix::FromRaw(a.players[17].pos.x.Raw() + 1);

    std::array<uint8_t, trace::kRecordSize> ba{}, bb{};
    trace::SerializeRecord(a, MakeBusyInput(), ba);
    trace::SerializeRecord(b, MakeBusyInput(), bb);

    CHECK(trace::RecordHash(ba) != trace::RecordHash(bb));
}

TEST_CASE("Trace header round-trips and rejects what it should") {
    trace::Header h;
    h.seed         = 0xDEADBEEFu;
    h.record_count = 12345;
    h.profile      = trace::Profile::kAmiga;
    h.first_team   = 1;
    h.tick_hz      = 50;

    std::array<uint8_t, trace::kHeaderSize> buf{};
    CHECK(trace::SerializeHeader(h, buf) == trace::kHeaderSize);

    trace::Header out;
    REQUIRE(trace::DeserializeHeader(buf, out));
    CHECK(out.seed == h.seed);
    CHECK(out.record_count == h.record_count);
    CHECK(out.profile == h.profile);
    CHECK(out.first_team == h.first_team);
    CHECK(out.tick_hz == h.tick_hz);

    SUBCASE("wrong magic is rejected") {
        auto bad = buf;
        bad[0] ^= 0xFF;
        trace::Header dummy;
        CHECK_FALSE(trace::DeserializeHeader(bad, dummy));
    }
    SUBCASE("wrong version is rejected rather than misread") {
        auto bad = buf;
        bad[4] = 0xFF;
        trace::Header dummy;
        CHECK_FALSE(trace::DeserializeHeader(bad, dummy));
    }
    SUBCASE("a stride from another build is rejected") {
        auto bad = buf;
        bad[6] ^= 0x01;
        trace::Header dummy;
        CHECK_FALSE(trace::DeserializeHeader(bad, dummy));
    }
}

TEST_CASE("Trace encoding is explicitly little-endian") {
    MatchState s{};
    s.tick = 0x01020304u;
    s.ball.pos.x = Fix::FromRaw(static_cast<int32_t>(0xAABBCCDDu));

    std::array<uint8_t, trace::kRecordSize> buf{};
    trace::SerializeRecord(s, MatchInput{}, buf);

    CHECK(buf[0] == 0x04);
    CHECK(buf[1] == 0x03);
    CHECK(buf[2] == 0x02);
    CHECK(buf[3] == 0x01);

    const size_t ball_at = trace::kRecordPrefixSize;
    CHECK(buf[ball_at + 0] == 0xDD);
    CHECK(buf[ball_at + 1] == 0xCC);
    CHECK(buf[ball_at + 2] == 0xBB);
    CHECK(buf[ball_at + 3] == 0xAA);
}

TEST_CASE("Trace serialisation is constexpr-usable and allocation-free") {
    constexpr bool ok = [] {
        std::array<uint8_t, trace::kRecordSize> buf{};
        MatchState s{};
        s.tick = 42;
        if (trace::SerializeRecord(s, MatchInput{}, buf) != trace::kRecordSize)
            return false;
        MatchState out{};
        MatchInput in{};
        return trace::DeserializeRecord(buf, out, in) && out.tick == 42u;
    }();
    static_assert(ok);
    CHECK(ok);
}
