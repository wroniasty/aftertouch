#pragma once
#include "core/match_state.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

// Explicit-field FNV-1a over gameplay-relevant MatchState.
// Presentation RNG is excluded so cosmetics cannot invent divergences (A2 §6.2).

namespace at {

namespace hash_detail {

constexpr uint64_t MixBytes(uint64_t h, const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

template <typename T>
uint64_t MixPod(uint64_t h, const T& v) {
    static_assert(std::has_unique_object_representations_v<T>);
    const auto* p = reinterpret_cast<const uint8_t*>(&v);
    return MixBytes(h, p, sizeof(T));
}

} // namespace hash_detail

inline uint64_t HashState(const MatchState& s) {
    uint64_t h = 1469598103934665603ull;
    h = hash_detail::MixPod(h, s.tick);
    h = hash_detail::MixPod(h, s.phase);
    h = hash_detail::MixPod(h, s.last_roll);
    h = hash_detail::MixPod(h, s.ball);
    h = hash_detail::MixPod(h, s.players);
    h = hash_detail::MixPod(h, s.referee);
    h = hash_detail::MixPod(h, s.booked_indicator);
    h = hash_detail::MixPod(h, s.score);
    h = hash_detail::MixPod(h, s.sides);
    h = hash_detail::MixPod(h, s.globals);
    h = hash_detail::MixPod(h, s.clock);
    h = hash_detail::MixPod(h, s.gameplay_rng);
    h = hash_detail::MixPod(h, s.resolve_rng);
    // presentation_rng intentionally omitted
    return h;
}

} // namespace at
