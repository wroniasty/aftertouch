#pragma once
#include <cstdint>
#include <vector>

namespace at::harness {

// RGBA buffer + content hash. C1 will fill this from an SDL_Renderer target; A6
// fixes the hash contract so presentation tests have a stable place to land.
struct RgbaSurface {
    int                  w = 0;
    int                  h = 0;
    std::vector<uint8_t> px;   // w*h*4, RGBA

    static RgbaSurface Solid(int width, int height, uint8_t r, uint8_t g, uint8_t b,
                             uint8_t a = 255) {
        RgbaSurface s;
        s.w = width;
        s.h = height;
        s.px.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
        for (size_t i = 0; i < s.px.size(); i += 4) {
            s.px[i + 0] = r;
            s.px[i + 1] = g;
            s.px[i + 2] = b;
            s.px[i + 3] = a;
        }
        return s;
    }

    uint64_t Hash() const {
        uint64_t acc = 1469598103934665603ull;
        auto mix = [&](uint64_t v) {
            acc ^= v;
            acc *= 1099511628211ull;
        };
        mix(static_cast<uint64_t>(w));
        mix(static_cast<uint64_t>(this->h));
        for (uint8_t b : px) mix(b);
        return acc;
    }
};

} // namespace at::harness
