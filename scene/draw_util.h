// draw_util.h: colours and primitive shapes shared by the railway's drawing.
#pragma once

#include <algorithm>
#include <cmath>

#include "strata_kit.h"

namespace railway {

using strata_kit::Rgb;

constexpr float kPi = 3.14159265f;

const Rgb kSteel{0.55f, 0.56f, 0.60f};
const Rgb kSleeper{0.22f, 0.15f, 0.10f};
const Rgb kBallast{0.36f, 0.34f, 0.31f};
const Rgb kIron{0.08f, 0.08f, 0.09f};
const Rgb kBrass{0.85f, 0.62f, 0.22f};
const Rgb kBufferRed{0.55f, 0.08f, 0.06f};
const Rgb kRedBeam{0.75f, 0.10f, 0.08f};
const Rgb kWhite{1.0f, 1.0f, 1.0f};
const Rgb kSteam{0.95f, 0.95f, 0.98f};

const Rgb kSignalGreen{0.10f, 1.00f, 0.35f};
const Rgb kSignalAmber{1.00f, 0.62f, 0.05f};
const Rgb kSignalRed{1.00f, 0.06f, 0.04f};

inline Rgb mix(Rgb a, Rgb b, float k) { return {a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k}; }
inline Rgb scale(Rgb a, float k) { return {a.r * k, a.g * k, a.b * k}; }

inline float fract(float v) { return v - std::floor(v); }
inline float hash(float a, float b) { return fract(std::sin(a * 127.1f + b * 311.7f) * 43758.5453f); }
// 0 when the viewer prefers reduced motion: animations then hold a still pose.
inline float motion() { return strata_kit::host().reducedMotion ? 0.0f : 1.0f; }

inline void box(float x, float y, float z, float sx, float sy, float sz, Rgb c) {
    strata_kit::host().add(STRATA_PRIMITIVE_CUBE, x, y, z, sx, sy, sz, c);
}
inline void ball(float x, float y, float z, float sx, float sy, float sz, Rgb c) {
    strata_kit::host().add(STRATA_PRIMITIVE_SPHERE, x, y, z, sx, sy, sz, c);
}
inline void cyl(float x, float y, float z, float r, float halfH, Rgb c) {
    strata_kit::host().add(STRATA_PRIMITIVE_CYLINDER, x, y, z, r * 2.0f, halfH, r * 2.0f, c);
}
inline void glow(float x, float y, float z, float px, float world, Rgb c, float a) {
    strata_kit::host().point(x, y, z, px, world, c, a, STRATA_POINT_SOFT);
}
inline void line(float x0, float y0, float z0, float x1, float y1, float z1, Rgb c, float a) {
    strata_kit::host().line(x0, y0, z0, x1, y1, z1, c, a);
}
inline void wline(float x0, float y0, float z0, float x1, float y1, float z1, Rgb c, float px, float world,
                  float a = 1.0f) {
    strata_kit::host().wideLine(x0, y0, z0, x1, y1, z1, c, px, world, a);
}
inline void light(float x, float y, float z, float range, Rgb c, float intensity, int priority) {
    strata_kit::host().light(x, y, z, range, c, intensity, priority);
}

// A horizontal ellipse of thin lines (rx == rz for a circle), turned by `spin`.
inline void ring(float x, float y, float z, float rx, float rz, Rgb c, float a, int segs = 20, float spin = 0.0f) {
    for (int i = 0; i < segs; ++i) {
        const float a0 = i * 2 * kPi / segs + spin, a1 = (i + 1) * 2 * kPi / segs + spin;
        line(x + std::cos(a0) * rx, y, z + std::sin(a0) * rz, x + std::cos(a1) * rx, y, z + std::sin(a1) * rz, c, a);
    }
}
// The same in wide lines.
inline void wideRing(float x, float y, float z, float rx, float rz, Rgb c, float px, float world, float a, int segs,
                     float spin = 0.0f) {
    for (int i = 0; i < segs; ++i) {
        const float a0 = i * 2 * kPi / segs + spin, a1 = (i + 1) * 2 * kPi / segs + spin;
        wline(x + std::cos(a0) * rx, y, z + std::sin(a0) * rz, x + std::cos(a1) * rx, y, z + std::sin(a1) * rz, c, px,
              world, a);
    }
}

}  // namespace railway
