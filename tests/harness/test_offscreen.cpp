#include <doctest/doctest.h>

#include "harness/offscreen.hpp"

using namespace at::harness;

TEST_CASE("offscreen surface hash is stable and content-sensitive") {
    const auto a = RgbaSurface::Solid(8, 4, 10, 20, 30);
    const auto b = RgbaSurface::Solid(8, 4, 10, 20, 30);
    const auto c = RgbaSurface::Solid(8, 4, 11, 20, 30);
    CHECK(a.Hash() == b.Hash());
    CHECK(a.Hash() != c.Hash());
    CHECK(a.px.size() == 8u * 4u * 4u);
}
