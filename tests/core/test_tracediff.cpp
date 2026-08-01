#include <doctest/doctest.h>

#include <string>

#include "tracekit/tracekit.hpp"

using namespace at;
using namespace at::tracekit;

TEST_CASE("tracediff: self-diff reports no divergence") {
    const Scenario s = KickoffScenario(40);
    std::vector<uint8_t> a;
    REQUIRE(Generate(s, a));
    const DiffResult r = Diff(a, a);
    CHECK(r.identical);
    CHECK(r.drift.empty());
}

TEST_CASE("tracediff: perturbation at tick k reports k and field class") {
    const Scenario s = KickoffScenario(40);
    std::vector<uint8_t> a, b;
    REQUIRE(Generate(s, a));
    b = a;

    // Tick index 12 → ball.pos.x first byte (entity 0, Fix field 0).
    constexpr uint32_t kIndex = 12;
    const size_t off = trace::kHeaderSize + kIndex * trace::kRecordSize + 12;
    b[off] ^= 0x01;

    const DiffResult r = Diff(a, b);
    CHECK_FALSE(r.identical);
    MatchState sa;
    MatchInput ia;
    REQUIRE(trace::DeserializeRecord(
        std::span<const uint8_t>(a).subspan(trace::kHeaderSize + kIndex * trace::kRecordSize,
                                            trace::kRecordSize),
        sa, ia));
    CHECK(r.tick == sa.tick);
    CHECK(r.first_class == FieldClass::Position);
    CHECK_FALSE(r.drift.empty());
}

TEST_CASE("tracediff: one raw unit is a divergence (zero tolerance)") {
    const Scenario s = KickoffScenario(20);
    std::vector<uint8_t> eng, stub;
    REQUIRE(Generate(s, eng));
    REQUIRE(Generate(s, stub, &StubOracleMutate));
    const DiffResult r = Diff(eng, stub);
    CHECK_FALSE(r.identical);
    CHECK(r.tick >= 1);
    CHECK(r.first_class == FieldClass::Position);
    // Drift should persist for remaining ticks.
    CHECK(r.drift.size() >= 1);
    CHECK(r.drift.front().l1_position >= 1);
}

TEST_CASE("tracediff: truncated and length-mismatched traces are errors") {
    const Scenario s = KickoffScenario(10);
    std::vector<uint8_t> a, b;
    REQUIRE(Generate(s, a));
    b = a;
    b.resize(b.size() / 2);
    CHECK_FALSE(Diff(a, b).identical);
    CHECK(Diff(a, b).first_class == FieldClass::Header);

    auto short_s = KickoffScenario(5);
    std::vector<uint8_t> c;
    REQUIRE(Generate(short_s, c));
    const DiffResult r = Diff(a, c);
    CHECK_FALSE(r.identical);
    CHECK(std::string(r.reason).find("count") != std::string::npos);
}
