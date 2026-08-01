#include <doctest/doctest.h>

#include <array>
#include <set>

#include "core/rng.hpp"

using namespace at;

TEST_CASE("kRandomTable is a permutation of 0..255") {
    std::set<uint8_t> seen;
    for (uint8_t v : kRandomTable) seen.insert(v);
    CHECK(seen.size() == 256u);
}

TEST_CASE("RngStream period is exactly 65536") {
    RngStream a;
    a.Seed(1);
    for (int i = 0; i < 65536; ++i) (void)a.Draw();

    RngStream b;
    b.Seed(1);
    CHECK(a.seed == b.seed);
    CHECK(a.xor_key == b.xor_key);
    CHECK(a.xor_index == b.xor_index);
    CHECK(a.Draw() == b.Draw());
}

TEST_CASE("RngStream wrap re-keys when seed hits zero") {
    RngStream r;
    r.Seed(0xFF);
    (void)r.Draw();
    CHECK(r.seed == 0);
    const uint8_t index_before = r.xor_index;
    (void)r.Draw();
    CHECK(r.xor_index == static_cast<uint8_t>(index_before + 1));
    CHECK(r.xor_key == kRandomTable[r.xor_index]);
}

TEST_CASE("two streams are independent") {
    RngStream g, p;
    g.Seed(0x1234);
    p.Seed(0x1234);
    (void)g.Draw();
    (void)g.Draw();
    (void)g.Draw();
    CHECK(p.seed == static_cast<uint8_t>(0x1234 & 0xFF));
    RngStream fresh;
    fresh.Seed(0x1234);
    CHECK(p.Draw() == fresh.Draw());
}

TEST_CASE("Seed is idempotent") {
    RngStream a, b;
    a.Seed(0xAABB);
    b.Seed(0xAABB);
    std::array<uint8_t, 64> ga{}, gb{};
    for (int i = 0; i < 64; ++i) {
        ga[static_cast<size_t>(i)] = a.Draw();
        gb[static_cast<size_t>(i)] = b.Draw();
    }
    CHECK(ga == gb);

    a.Seed(0xAABB);
    for (int i = 0; i < 64; ++i)
        CHECK(a.Draw() == ga[static_cast<size_t>(i)]);
}

TEST_CASE("golden first-64 draws for seed 1") {
    RngStream r;
    r.Seed(1);
    const uint8_t expect[64] = {
        0xd5, 0x4e, 0x20, 0x34, 0x44, 0x80, 0xea, 0xe8, 0x86, 0x78, 0x97, 0x04, 0x9d, 0xaf, 0x74,
        0x30, 0xbd, 0x8d, 0x37, 0xce, 0xac, 0x5d, 0x01, 0x38, 0xc2, 0xf0, 0x5a, 0xc0, 0x7f, 0x48,
        0xf2, 0x5f, 0x59, 0x7c, 0x2e, 0x08, 0xa8, 0xbc, 0x19, 0x70, 0x89, 0x6e, 0x10, 0x90, 0x39,
        0x26, 0x1f, 0x64, 0x24, 0x18, 0x2f, 0xc5, 0x4f, 0x32, 0x23, 0x46, 0x3e, 0xe2, 0x3a, 0xef,
        0xba, 0xc1, 0xd0, 0x72,
    };
    for (int i = 0; i < 64; ++i)
        CHECK(r.Draw() == expect[i]);
}

TEST_CASE("upper 16 bits of seed are ignored") {
    RngStream a, b;
    a.Seed(0x00001234);
    b.Seed(0xFFFF1234);
    for (int i = 0; i < 16; ++i)
        CHECK(a.Draw() == b.Draw());
}
