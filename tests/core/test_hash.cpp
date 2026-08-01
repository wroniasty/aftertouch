#include <doctest/doctest.h>

#include <type_traits>

#include "core/hash.hpp"

using namespace at;

TEST_CASE("MatchState has unique object representation") {
    CHECK(std::has_unique_object_representations_v<MatchState>);
    CHECK(std::has_unique_object_representations_v<EntityState>);
}

TEST_CASE("HashState: default states are equal") {
    MatchState a{}, b{};
    CHECK(HashState(a) == HashState(b));
}

TEST_CASE("HashState: one-bit change in any hashed field changes the hash") {
    const MatchState base{};
    const uint64_t h0 = HashState(base);

    {
        MatchState s = base;
        s.tick = 1;
        CHECK(HashState(s) != h0);
    }
    {
        MatchState s = base;
        s.ball.pos.x = Fix::FromRaw(1);
        CHECK(HashState(s) != h0);
    }
    {
        MatchState s = base;
        s.players[3].speed = 1;
        CHECK(HashState(s) != h0);
    }
    {
        MatchState s = base;
        s.score[1] = 1;
        CHECK(HashState(s) != h0);
    }
    {
        MatchState s = base;
        s.gameplay_rng.Seed(1);
        CHECK(HashState(s) != h0);
    }
}

TEST_CASE("HashState ignores presentation RNG") {
    MatchState a{}, b{};
    a.presentation_rng.Seed(1);
    b.presentation_rng.Seed(2);
    CHECK(HashState(a) == HashState(b));
}
