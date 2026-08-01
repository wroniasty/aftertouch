#include <doctest/doctest.h>

#include <cstring>

#include "tracekit/tracekit.hpp"

using namespace at;
using namespace at::tracekit;

TEST_CASE("tracegen: same input log is byte-identical across two Generate calls") {
    const Scenario s = KickoffScenario(60);
    std::vector<uint8_t> a, b;
    REQUIRE(Generate(s, a));
    REQUIRE(Generate(s, b));
    REQUIRE(a.size() == b.size());
    CHECK(std::memcmp(a.data(), b.data(), a.size()) == 0);
    CHECK(Diff(a, b).identical);
}

TEST_CASE("tracegen: input log round-trips and regenerates identically") {
    const Scenario s = ShotCurlScenario(40);
    std::vector<uint8_t> atin;
    REQUIRE(SerializeInputLog(s, atin));

    Scenario loaded;
    REQUIRE(DeserializeInputLog(atin, loaded));
    CHECK(loaded.seed == s.seed);
    CHECK(loaded.inputs.size() == s.inputs.size());

    std::vector<uint8_t> from_orig, from_loaded;
    REQUIRE(Generate(s, from_orig));
    REQUIRE(Generate(loaded, from_loaded));
    CHECK(Diff(from_orig, from_loaded).identical);
    CHECK(HashChain(from_orig) == HashChain(from_loaded));
    CHECK(HashChain(from_orig) != 0);
}

TEST_CASE("tracegen: stub oracle diverges from engine at first physics tick") {
    const Scenario s = KickoffScenario(10);
    std::vector<uint8_t> eng, stub;
    REQUIRE(Generate(s, eng));
    REQUIRE(Generate(s, stub, &StubOracleMutate));
    const DiffResult r = Diff(eng, stub);
    CHECK_FALSE(r.identical);
    CHECK(r.tick == 1);
}
