// B4: MoveSprite axis snap.
#include <doctest/doctest.h>

#include "core/movement.hpp"

using namespace at;

TEST_CASE("axis snaps independently on arrival") {
    Entity e{};
    e.pos.x = Fix::FromInt(100);
    e.pos.y = Fix::FromInt(200);
    e.dest_x = 105;
    e.dest_y = 200;
    e.delta.x = Fix::FromInt(10); // overshoots x
    e.delta.y = Fix{};

    MoveSprite(e);

    CHECK(e.pos.x.Whole() == 105);
    CHECK(e.delta.x.Raw() == 0);
    CHECK(e.pos.y.Whole() == 200);
}

TEST_CASE("y axis snaps without touching x") {
    Entity e{};
    e.pos.x = Fix::FromInt(50);
    e.pos.y = Fix::FromInt(50);
    e.dest_x = 80;
    e.dest_y = 55;
    e.delta.x = Fix::FromInt(2);
    e.delta.y = Fix::FromInt(10); // overshoots y

    MoveSprite(e);

    CHECK(e.pos.x.Whole() == 52);
    CHECK(e.delta.x.Raw() != 0);
    CHECK(e.pos.y.Whole() == 55);
    CHECK(e.delta.y.Raw() == 0);
}
