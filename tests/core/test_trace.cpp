// A3 work item 1: the trace record format.
//
// These tests are about the instrument being trustworthy, which is a different
// question from the engine being right. A trace format that silently misreads is
// worse than no trace format, because it produces a number that looks like a
// measurement. See doc/implementation/A3-trace-harness.md section 5.
#include <doctest/doctest.h>

#include <array>
#include <cstring>
#include <vector>

#include "core/trace.hpp"

using namespace at;

namespace {

// A state with every field distinct, so that a serialiser which drops or
// transposes one is caught. Default-constructed states are all zeroes and would
// round-trip through almost any bug.
MatchState MakeBusyState() {
    MatchState s{};
    s.tick  = 0x01020304u;
    s.phase = MatchPhase::Goal;
    s.score = {3, 5};

    s.ball.pos = {Fix::FromRaw(11), Fix::FromRaw(-22), Fix::FromRaw(33)};
    s.ball.vel = {Fix::FromRaw(-44), Fix::FromRaw(55), Fix::FromRaw(-66)};
    s.ball.anim_frame = 7;
    s.ball.flags      = 9;

    for (size_t i = 0; i < s.players.size(); ++i) {
        const int32_t n = static_cast<int32_t>(i) + 1;
        s.players[i].pos = {Fix::FromRaw(n * 100), Fix::FromRaw(-n * 200),
                            Fix::FromRaw(n * 300)};
        s.players[i].vel = {Fix::FromRaw(-n * 400), Fix::FromRaw(n * 500),
                            Fix::FromRaw(-n * 600)};
        s.players[i].anim_frame = static_cast<uint8_t>(n);
        s.players[i].flags      = static_cast<uint8_t>(255 - n);
    }
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

bool SameState(const MatchState& a, const MatchState& b) {
    auto same_entity = [](const EntityState& x, const EntityState& y) {
        return x.pos.x == y.pos.x && x.pos.y == y.pos.y && x.pos.z == y.pos.z &&
               x.vel.x == y.vel.x && x.vel.y == y.vel.y && x.vel.z == y.vel.z &&
               x.anim_frame == y.anim_frame && x.flags == y.flags;
    };
    if (a.tick != b.tick || a.phase != b.phase || a.score != b.score) return false;
    if (!same_entity(a.ball, b.ball)) return false;
    for (size_t i = 0; i < a.players.size(); ++i)
        if (!same_entity(a.players[i], b.players[i])) return false;
    return true;
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

    CHECK(SameState(s, out));
    CHECK(out_in.p1.dir == in.p1.dir);
    CHECK(out_in.p1.fire == in.p1.fire);
    CHECK(out_in.p2.dir == in.p2.dir);
    CHECK(out_in.p2.fire == in.p2.fire);
}

TEST_CASE("Trace record is fixed width, so tick maps to offset by arithmetic") {
    // The differ depends on this; if the stride ever stops being constant the
    // seek arithmetic becomes a silent misread rather than a compile error.
    const MatchState s  = MakeBusyState();
    const MatchInput in = MakeBusyInput();

    std::array<uint8_t, trace::kRecordSize> a{}, b{};
    MatchState empty{};
    CHECK(trace::SerializeRecord(s, in, a) == trace::SerializeRecord(empty, in, b));
    CHECK(trace::kRecordSize == 4 + 1 + 4 + 2 + 1 + trace::kEntitySize * 23 + 8);
}

TEST_CASE("Trace record refuses a buffer that is too small") {
    std::array<uint8_t, trace::kRecordSize - 1> small{};
    CHECK(trace::SerializeRecord(MakeBusyState(), MakeBusyInput(), small) == 0);

    MatchState s{};
    MatchInput in{};
    CHECK_FALSE(trace::DeserializeRecord(small, s, in));
}

// The hash exists so a corrupt record is rejected rather than believed. A trace
// harness that trusts its input can report a divergence that is really a bad
// read, which is the most expensive kind of wrong answer this tool can give.
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
    b.players[17].pos.x = Fix::FromRaw(a.players[17].pos.x.Raw() + 1);  // one raw unit

    std::array<uint8_t, trace::kRecordSize> ba{}, bb{};
    trace::SerializeRecord(a, MakeBusyInput(), ba);
    trace::SerializeRecord(b, MakeBusyInput(), bb);

    // A one-raw-unit difference is a divergence. A3 section 2.6: the tolerance is
    // zero, and this is that rule asserted rather than described.
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

// Byte-level pinning. The point of hand-rolled little-endian writes is that a
// trace recorded on one platform is byte-identical on the other; a test that only
// round-trips through the same code would pass on a big-endian machine that wrote
// the wrong bytes.
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

    const size_t ball_at = 4 + 1 + 4 + 2 + 1;
    CHECK(buf[ball_at + 0] == 0xDD);
    CHECK(buf[ball_at + 1] == 0xCC);
    CHECK(buf[ball_at + 2] == 0xBB);
    CHECK(buf[ball_at + 3] == 0xAA);
}

TEST_CASE("Trace serialisation is constexpr-usable and allocation-free") {
    // Rule 1: no I/O, no allocation. If this ever stops compiling as a constant
    // expression, something in the path started allocating or touching the world.
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
