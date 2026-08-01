#include <doctest/doctest.h>

#include "core/profile.hpp"
#include "core/trig.hpp"
#include "core/trig_tables.hpp"

using namespace at;

TEST_CASE("Amiga is the default platform profile") {
    CHECK(kPlatformProfile == PlatformProfile::Amiga);
    CHECK(IsAmigaProfile());
}

TEST_CASE("CalculateDeltaXAndY: no movement when dest == from") {
    const auto r = CalculateDeltaXAndY(1024, Dest{10, 20}, Dest{10, 20});
    CHECK(r.heading == -1);
    CHECK(r.dx.Raw() == 0);
    CHECK(r.dy.Raw() == 0);
}

TEST_CASE("CalculateDeltaXAndY: due north") {
    // Dest 1000 units up from origin — table index folds to heading 0.
    const auto r = CalculateDeltaXAndY(1024, Dest{0, 0}, Dest{0, -1000});
    CHECK(r.heading == 0);
    CHECK(r.dx.Raw() == 0);
    // sin/cos after (v>>8)*speed: for heading 0 (up), dy should be negative.
    CHECK(r.dy.Raw() < 0);
}

TEST_CASE("CalculateDeltaXAndY: due east") {
    const auto r = CalculateDeltaXAndY(1024, Dest{0, 0}, Dest{1000, 0});
    CHECK(r.heading == 64);
    CHECK(r.dx.Raw() > 0);
    CHECK(r.dy.Raw() == 0);
}

TEST_CASE("CalculateDeltaXAndY: domain reduction on a long vector") {
    // Same direction as a short vector; heading must agree after halvings.
    const auto near = CalculateDeltaXAndY(256, Dest{0, 0}, Dest{16, 0});
    const auto far  = CalculateDeltaXAndY(256, Dest{0, 0}, Dest{1000, 0});
    CHECK(near.heading == far.heading);
    CHECK(near.heading == 64);
}

TEST_CASE("CalculateDeltaXAndY: diagonal isotropy at 45°") {
    const auto r = CalculateDeltaXAndY(1024, Dest{0, 0}, Dest{1000, -1000});
    // NE → heading 32. Table entries at the complementary indices are ±23170
    // (A2 §5); after (v>>8)*speed the arithmetic shift makes |sin| and |cos|
    // differ by one high-byte step, so we pin heading + signs, not raw equality.
    CHECK(r.heading == 32);
    CHECK(r.dx.Raw() > 0);
    CHECK(r.dy.Raw() < 0);
    CHECK(kSineCosineTable[96] == 23170);
    CHECK(kSineCosineTable[160] == -23170);
}

TEST_CASE("AngleTo is CalculateDeltaXAndY at speed 256") {
    const Dest a{0, 0};
    const Dest b{100, 50};
    CHECK(AngleTo(a, b) == CalculateDeltaXAndY(256, a, b).heading);
}
