// A6: tracekit generate + diff (no golden file — pure synthetic).
#include <doctest/doctest.h>

#include "tracekit/tracekit.hpp"

using namespace at;
using namespace at::tracekit;

TEST_CASE("tracekit self-diff of a generated scenario is identical") {
    const Scenario s = KickoffScenario(50);
    std::vector<uint8_t> a, b;
    REQUIRE(Generate(s, a));
    REQUIRE(Generate(s, b));
    const DiffResult r = Diff(a, b);
    CHECK(r.identical);
    CHECK(a.size() == trace::kHeaderSize + 50 * trace::kRecordSize);
}

TEST_CASE("tracekit reports divergence when a record is perturbed") {
    const Scenario s = KickoffScenario(30);
    std::vector<uint8_t> a, b;
    REQUIRE(Generate(s, a));
    b = a;
    // Flip one payload byte in tick index 7 (not the trailing hash — corrupt hash
    // would also fail decode; we want a content mismatch Diff can name).
    const size_t off = trace::kHeaderSize + 7 * trace::kRecordSize;
    b[off + 4] ^= 0x01;   // phase / early field
    // Recompute would be needed for a valid record; Diff compares hashes first,
    // so a raw flip is enough to report divergence.
    const DiffResult r = Diff(a, b);
    CHECK_FALSE(r.identical);
    CHECK(r.reason != nullptr);
}

TEST_CASE("tracekit rejects truncated traces") {
    const Scenario s = KickoffScenario(10);
    std::vector<uint8_t> a;
    REQUIRE(Generate(s, a));
    a.resize(a.size() / 2);
    const DiffResult r = Diff(a, a);
    CHECK_FALSE(r.identical);
}
