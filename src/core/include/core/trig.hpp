#pragma once
#include "core/angle.hpp"
#include "core/fixed.hpp"
#include "core/profile.hpp"
#include "core/trig_tables.hpp"

#include <cstdint>

// Movement kernel: destination + speed -> per-tick delta.
// See doc/implementation/A2-determinism-primitives.md section 2.4.
//
// The only reader of kSineCosineTable / kAngleTangent. B parts call this; they
// never index the tables directly.

namespace at {

struct DeltaResult {
    Fix     dx;
    Fix     dy;
    int16_t heading = -1;  // Heading 0..255, or -1 for no movement
};

// from/dest are whole-unit coordinates (typically Fix::Whole() of a position).
inline DeltaResult CalculateDeltaXAndY(int speed, Dest from, Dest dest) {
    DeltaResult result;

    int delta_x = static_cast<int>(dest.x) - static_cast<int>(from.x);
    int delta_y = static_cast<int>(dest.y) - static_cast<int>(from.y);

    bool x_neg = false;
    bool y_neg = false;
    if (delta_x < 0) {
        x_neg = true;
        delta_x = -delta_x;
    }
    if (delta_y < 0) {
        y_neg = true;
        delta_y = -delta_y;
    }

    while (delta_x >= 32 || delta_y >= 32) {
        delta_x /= 2;
        delta_y /= 2;
    }

    const int angle_q = kAngleTangent[static_cast<size_t>(delta_y)]
                                     [static_cast<size_t>(delta_x)];
    if (angle_q < 0) return result;  // no movement: zeros, heading -1

    // Fold first-quadrant angle into table-index space (0 = down, CCW).
    int angle = angle_q;
    if (x_neg) {
        if (y_neg)
            angle = 192 - angle;
        else
            angle += 192;
    } else {
        if (y_neg)
            angle += 64;
        else
            angle = 64 - angle;
    }
    angle &= 0xff;

    result.heading = static_cast<int16_t>((256 - angle + 128) & 0xff);

    int cos = kSineCosineTable[static_cast<size_t>(angle)];
    int sin = kSineCosineTable[static_cast<size_t>((angle + 64) & 0xff)];

    // SWOS_TEST / original form — NOT the port's default "more precision" path.
    sin = (sin >> 8) * speed;
    cos = (cos >> 8) * speed;

    if (!IsAmigaProfile()) {
        // 41/64 PC frame-rate compensation, shift chain for bit-exact rounding.
        sin = sin - (sin >> 2) - (sin >> 4) - (sin >> 5) - (sin >> 6);
        cos = cos - (cos >> 2) - (cos >> 4) - (cos >> 5) - (cos >> 6);
    }

    result.dy = Fix::FromRaw(sin);
    result.dx = Fix::FromRaw(cos);
    return result;
}

// Angle-only wrapper at the dummy speed of 256 used by AI/facing callers.
inline int16_t AngleTo(Dest from, Dest dest) {
    return CalculateDeltaXAndY(256, from, dest).heading;
}

} // namespace at
