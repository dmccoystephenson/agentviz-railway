// agentviz-railway: Claude Code sessions as a railway yard, on the Strata web kit.
//
// The page (web/railway.js) owns the data and the layout: it polls
// /api/agents, eases between snapshots and, every frame, hands this file
// three flat float arrays:
//   rails   [x0, z0, x1, z1, kind]                 kind 0 trunk, 1 siding/ladder
//   trains  [x, z, scale, dir, state, action, seed, flags, speed, lead]
//   props   [type, x, z, a, b, c]                  sheds, switches, buffers, glows, stations
// and this file draws them: track, locomotives, signals and what each one is
// doing. Every animation comes from scene time, so ?drive=1 captures repeat.
#define STRATA_KIT_MAIN
#include "strata_kit.h"

#include <cmath>
#include <vector>

namespace {

using strata_kit::host;
using strata_kit::Rgb;

constexpr float kPi = 3.14159265f;
constexpr int kRailStride = 5;
constexpr int kTrainStride = 10;
constexpr int kPropStride = 6;

// Train states (railway.js STATE_CODE).
enum { ST_OFFLINE = 0, ST_DIM = 1, ST_IDLE = 2, ST_BUSY = 3, ST_WAITING = 4, ST_RESTING = 5 };
// Actions (railway.js ACTION_CODE).
enum { AC_NONE = 0, AC_EDIT = 1, AC_RUN = 2, AC_READ = 3, AC_WEB = 4, AC_DELEGATE = 5, AC_THINK = 6, AC_OTHER = 7 };
// Train flags.
enum { FL_SELECTED = 1, FL_STALLED = 2, FL_HOVER = 4 };
// Prop types.
enum { PR_SHED = 0, PR_SWITCH = 1, PR_BUFFER = 2, PR_GLOW = 3, PR_STATION = 4, PR_GROUND = 5 };

struct Yard {
    std::vector<float> rails, trains, props;
} g;

const Rgb kSteel{0.55f, 0.56f, 0.60f};
const Rgb kSleeper{0.22f, 0.15f, 0.10f};
const Rgb kBallast{0.36f, 0.34f, 0.31f};
const Rgb kIron{0.08f, 0.08f, 0.09f};
const Rgb kBrass{0.85f, 0.62f, 0.22f};
const Rgb kRedBeam{0.75f, 0.10f, 0.08f};

const Rgb kSignalGreen{0.10f, 1.00f, 0.35f};
const Rgb kSignalAmber{1.00f, 0.62f, 0.05f};
const Rgb kSignalRed{1.00f, 0.06f, 0.04f};

// Liveries, chosen by seed: deep green, maroon, blue, black, teal, ochre.
const Rgb kLivery[6] = {{0.08f, 0.32f, 0.16f}, {0.42f, 0.08f, 0.08f}, {0.10f, 0.20f, 0.48f},
                        {0.14f, 0.14f, 0.16f}, {0.05f, 0.36f, 0.38f}, {0.55f, 0.38f, 0.08f}};

Rgb mix(Rgb a, Rgb b, float k) { return {a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k}; }
Rgb scale(Rgb a, float k) { return {a.r * k, a.g * k, a.b * k}; }

float fract(float v) { return v - std::floor(v); }
float hash(float a, float b) { return fract(std::sin(a * 127.1f + b * 311.7f) * 43758.5453f); }
float motion() { return host().reducedMotion ? 0.0f : 1.0f; }

void box(float x, float y, float z, float sx, float sy, float sz, Rgb c) {
    host().add(STRATA_PRIMITIVE_CUBE, x, y, z, sx, sy, sz, c);
}
void ball(float x, float y, float z, float sx, float sy, float sz, Rgb c) {
    host().add(STRATA_PRIMITIVE_SPHERE, x, y, z, sx, sy, sz, c);
}
void cyl(float x, float y, float z, float r, float halfH, Rgb c) {
    host().add(STRATA_PRIMITIVE_CYLINDER, x, y, z, r * 2.0f, halfH, r * 2.0f, c);
}
void glow(float x, float y, float z, float px, float world, Rgb c, float a) {
    host().point(x, y, z, px, world, c, a, STRATA_POINT_SOFT);
}
void wline(float x0, float y0, float z0, float x1, float y1, float z1, Rgb c, float px, float world, float a = 1.0f) {
    host().wideLine(x0, y0, z0, x1, y1, z1, c, px, world, a);
}

// ------------------------------------------------------------------ track

void buildRail(const float* r) {
    const float x0 = r[0], z0 = r[1], x1 = r[2], z1 = r[3];
    const bool trunk = r[4] < 0.5f;
    const float dx = x1 - x0, dz = z1 - z0, len = std::sqrt(dx * dx + dz * dz);
    if (len < 0.01f) return;
    const float ux = dx / len, uz = dz / len, nx = -uz, nz = ux;  // along, across
    const float gauge = 0.36f, ballastW = trunk ? 1.5f : 1.25f;
    const bool alongX = std::fabs(uz) < 0.001f;
    if (alongX) {
        box((x0 + x1) * 0.5f, 0.04f, z0, len + 0.4f, 0.12f, ballastW, kBallast);
        for (float s = 0.3f; s < len; s += 0.62f)
            box(x0 + ux * s, 0.12f, z0, 0.22f, 0.07f, 0.95f, kSleeper);
        for (int side = -1; side <= 1; side += 2)
            box((x0 + x1) * 0.5f, 0.2f, z0 + side * gauge, len, 0.09f, 0.07f, kSteel);
    } else {
        wline(x0, 0.06f, z0, x1, 0.06f, z1, kBallast, 4.0f, ballastW);
        for (float s = 0.3f; s < len; s += 0.62f) {
            const float cx = x0 + ux * s, cz = z0 + uz * s;
            wline(cx - nx * 0.48f, 0.13f, cz - nz * 0.48f, cx + nx * 0.48f, 0.13f, cz + nz * 0.48f, kSleeper, 2.0f,
                  0.2f);
        }
        for (int side = -1; side <= 1; side += 2)
            wline(x0 + nx * gauge * side, 0.21f, z0 + nz * gauge * side, x1 + nx * gauge * side, 0.21f,
                  z1 + nz * gauge * side, kSteel, 1.5f, 0.08f);
    }
}

// ------------------------------------------------------------------ props

void buildShed(float x0, float x1, float z, float depth) {
    // An engine shed over the head of the trunk, cut away on the near (+z)
    // side so the cold engines parked inside stay visible: back wall, end
    // wall, the far roof slab with smoke vents, and pillars with a beam in
    // front.
    const Rgb brick{0.42f, 0.20f, 0.14f}, roof{0.20f, 0.21f, 0.24f};
    const float w = x1 - x0, cx = (x0 + x1) * 0.5f, h = 2.9f, half = depth * 0.5f;
    box(cx, h * 0.5f, z - half, w, h, 0.25f, brick);
    box(x0, h * 0.5f, z, 0.25f, h, depth, brick);
    box(cx, h + 0.15f, z - half * 0.55f, w + 0.4f, 0.3f, half * 0.9f + 0.3f, roof);
    for (float px = x0 + 0.2f; px <= x1 + 0.01f; px += std::max(2.0f, w / std::max(1.0f, std::round(w / 4.5f))))
        box(px, h * 0.5f, z + half, 0.22f, h, 0.22f, scale(brick, 0.85f));
    box(cx, h - 0.1f, z + half, w, 0.25f, 0.25f, scale(brick, 0.85f));
    for (float vx = x0 + 1.5f; vx < x1 - 0.5f; vx += 3.0f) box(vx, h + 0.6f, z - half * 0.5f, 0.5f, 0.5f, 0.5f, kIron);
}

void buildSwitch(const float* p, double t) {
    // A ground lever beside a junction. a = active (the lead is delegating),
    // b = phase (stagger). Active levers throw back and forth and their lamp
    // flips white/green; idle ones rest.
    const float x = p[1], z = p[2], active = p[3], phase = p[4];
    box(x, 0.2f, z, 0.45f, 0.3f, 0.3f, scale(kIron, 2.0f));
    float throwK = 0.0f;
    if (active > 0.5f) {
        const float c = fract(static_cast<float>(t) * 0.55f + phase);
        throwK = motion() > 0 ? (c < 0.5f ? std::min(1.0f, c * 6.0f) : std::max(0.0f, 1.0f - (c - 0.5f) * 6.0f)) : 1.0f;
    }
    const float ang = (-0.6f + 1.2f * throwK);
    const float lx = x + std::sin(ang) * 0.75f, ly = 0.35f + std::cos(ang) * 0.75f;
    wline(x, 0.35f, z, lx, ly, z, {0.75f, 0.12f, 0.08f}, 3.0f, 0.08f);
    ball(lx, ly, z, 0.14f, 0.14f, 0.14f, {0.9f, 0.9f, 0.9f});
    const Rgb lamp = throwK > 0.5f ? kSignalGreen : Rgb{0.95f, 0.95f, 0.85f};
    ball(x, 0.75f, z - 0.05f, 0.18f, 0.18f, 0.18f, lamp);
    if (active > 0.5f) glow(x, 0.75f, z, 6.0f, 0.6f, lamp, 0.55f);
}

void buildBuffer(float x, float z, float s) {
    // A buffer stop at the end of a siding.
    box(x, 0.45f * s, z, 0.25f * s, 0.6f * s, 1.0f * s, {0.45f, 0.08f, 0.06f});
    for (int side = -1; side <= 1; side += 2) ball(x - 0.15f * s, 0.55f * s, z + side * 0.3f * s, 0.18f * s, 0.18f * s, 0.18f * s, kIron);
    box(x + 0.3f * s, 0.25f * s, z, 0.5f * s, 0.25f * s, 0.9f * s, kSleeper);
}

void buildStation(float x, float z, float seed) {
    // A small signal box at the head of each trunk: the place's station.
    const Rgb wall = mix({0.75f, 0.68f, 0.52f}, {0.62f, 0.55f, 0.45f}, seed);
    box(x, 1.0f, z - 2.2f, 2.2f, 2.0f, 1.6f, wall);
    box(x, 2.15f, z - 2.2f, 2.6f, 0.3f, 2.0f, {0.35f, 0.12f, 0.08f});
    box(x, 1.4f, z - 1.39f, 1.6f, 0.6f, 0.05f, {0.12f, 0.16f, 0.22f});
    // Nameboard posts.
    box(x - 0.9f, 0.6f, z - 1.2f, 0.1f, 1.2f, 0.1f, kIron);
    box(x + 0.9f, 0.6f, z - 1.2f, 0.1f, 1.2f, 0.1f, kIron);
    box(x, 1.15f, z - 1.2f, 2.0f, 0.4f, 0.08f, {0.92f, 0.92f, 0.88f});
}

// ------------------------------------------------------------------ trains

struct Loco {
    float x, z, s, dir;
    int state, action;
    float seed;
    int flags;
    float speed;
    bool lead;
};

// Offsets along the train are multiplied by dir (1 faces +x, -1 faces -x).
float ax(const Loco& L, float off) { return L.x + off * L.s * L.dir; }

void smoke(const Loco& L, double t, Rgb c, int n, float rate, float rise, float size, float alpha) {
    const float cx = ax(L, 1.2f), cy = 2.15f * L.s;
    for (int i = 0; i < n; ++i) {
        const float age = motion() > 0 ? fract(static_cast<float>(t) * rate + i / static_cast<float>(n) + L.seed)
                                       : (i + 0.5f) / n;
        const float jit = hash(L.seed * 31.0f + i, std::floor(static_cast<float>(t) * rate + i / static_cast<float>(n)));
        const float y = cy + age * rise * L.s;
        const float x = cx - L.dir * age * (1.2f + L.speed * 2.0f) * L.s + (jit - 0.5f) * 0.4f * L.s;
        const float z = L.z + (hash(jit, 3.0f) - 0.5f) * 0.6f * L.s * age;
        glow(x, y, z, 4.0f, (0.35f + age * size) * L.s, c, alpha * (1.0f - age));
    }
}

void wheel(const Loco& L, float off, float zside, float r, float spin, Rgb rim) {
    const float x = ax(L, off), y = r * L.s + 0.2f, z = L.z + zside * L.s;
    ball(x, y, z, 2 * r * L.s, 2 * r * L.s, 0.1f * L.s, rim);
    ball(x, y, z + (zside > 0 ? 0.04f : -0.04f) * L.s, 0.45f * r * L.s, 0.45f * r * L.s, 0.1f * L.s, kIron);
    const float zs = z + (zside > 0 ? 0.07f : -0.07f) * L.s;
    for (int k = 0; k < 3; ++k) {
        const float a = spin + k * kPi / 3.0f;
        const float ex = std::cos(a) * r * 0.9f * L.s, ey = std::sin(a) * r * 0.9f * L.s;
        host().line(x - ex, y - ey, zs, x + ex, y + ey, zs, {0.9f, 0.85f, 0.7f}, 0.9f);
    }
}

void buildSignal(const Loco& L, double t, Rgb& lampOut, bool& lit) {
    // A semaphore-style colour-light signal on the -z side, by the front.
    const float sx = ax(L, 2.3f), sz = L.z - 1.15f * L.s;
    const float h = (L.lead ? 2.6f : 2.0f) * L.s;
    cyl(sx, h * 0.5f, sz, 0.06f * L.s, h * 0.5f, kIron);
    box(sx, h, sz, 0.42f * L.s, 0.7f * L.s, 0.3f * L.s, kIron);
    Rgb lamp{0.04f, 0.04f, 0.04f};
    lit = true;
    float a = 0.85f;
    switch (L.state) {
        case ST_BUSY: lamp = kSignalGreen; break;
        case ST_IDLE: case ST_RESTING: lamp = kSignalAmber; break;
        case ST_WAITING: {
            const bool on = motion() == 0 || fract(static_cast<float>(t) * 2.0f) < 0.6f;
            lamp = on ? kSignalRed : Rgb{0.15f, 0.02f, 0.02f};
            lit = on;
            a = 1.0f;
            break;
        }
        case ST_DIM: lamp = {0.35f, 0.42f, 0.55f}; a = 0.4f; break;
        default: lit = false; break;
    }
    ball(sx - 0.0f, h, sz + 0.16f * L.s, 0.28f * L.s, 0.28f * L.s, 0.08f * L.s, lamp);
    ball(sx, h, sz - 0.16f * L.s, 0.28f * L.s, 0.28f * L.s, 0.08f * L.s, lamp);
    if (lit) {
        glow(sx, h, sz + 0.2f * L.s, 10.0f * (L.state == ST_WAITING ? 2.0f : 1.0f), 0.9f * L.s, lamp, a);
        host().light(sx, h + 0.2f, sz, 4.0f * L.s, lamp, 1.0f, L.state == ST_WAITING ? 9 : 2);
    }
    lampOut = lamp;
    if (L.state == ST_WAITING) {
        // The bell: a brass bell on a bracket above the signal, swinging,
        // with rings going out each strike; a tall red beam over the train.
        const float swing = motion() * std::sin(static_cast<float>(t) * 9.0f);
        const float by = h + 0.95f * L.s;
        box(sx, h + 0.55f * L.s, sz, 0.08f * L.s, 0.5f * L.s, 0.08f * L.s, kIron);
        box(sx, by + 0.35f * L.s, sz, 0.7f * L.s, 0.08f * L.s, 0.08f * L.s, kIron);
        ball(sx + swing * 0.18f * L.s, by, sz, 0.5f * L.s, 0.55f * L.s, 0.5f * L.s, kBrass);
        ball(sx + swing * 0.3f * L.s, by - 0.3f * L.s, sz, 0.14f * L.s, 0.14f * L.s, 0.14f * L.s, kIron);
        for (int k = 0; k < 3; ++k) {
            const float age = fract(static_cast<float>(t) * 1.4f + k / 3.0f);
            const float r = (0.4f + age * 2.4f) * L.s;
            for (int i = 0; i < 20; ++i) {
                const float a0 = i * 2 * kPi / 20, a1 = (i + 1) * 2 * kPi / 20;
                host().line(sx + std::cos(a0) * r, by, sz + std::sin(a0) * r, sx + std::cos(a1) * r, by,
                            sz + std::sin(a1) * r, kBrass, (1.0f - age) * 0.9f);
            }
        }
        const float pulse = 0.55f + 0.45f * std::sin(static_cast<float>(t) * 6.0f) * motion();
        wline(L.x, 0.3f, L.z, L.x, 14.0f, L.z, kRedBeam, 6.0f, 0.5f, 0.35f + 0.35f * pulse);
        glow(L.x, 7.0f, L.z, 30.0f, 3.0f, kSignalRed, 0.25f * pulse);
        for (int i = 0; i < 32; ++i) {
            const float a0 = i * 2 * kPi / 32 + static_cast<float>(t) * motion(), a1 = a0 + 2 * kPi / 32;
            const float r = 4.2f * L.s * (0.92f + 0.08f * pulse);
            wline(L.x + std::cos(a0) * r, 0.3f, L.z + std::sin(a0) * r, L.x + std::cos(a1) * r, 0.3f,
                  L.z + std::sin(a1) * r, kSignalRed, 4.0f, 0.18f, 0.9f);
        }
        host().light(L.x, 3.0f, L.z, 9.0f, kSignalRed, 2.5f * pulse, 10);
    }
}

void buildLoco(const Loco& L, double t) {
    const bool cold = L.state == ST_OFFLINE;
    const bool stalled = (L.flags & FL_STALLED) != 0;
    Rgb paint = kLivery[static_cast<int>(L.seed * 6.0f) % 6];
    if (cold) paint = mix(scale(paint, 0.5f), {0.18f, 0.18f, 0.2f}, 0.6f);
    if (L.state == ST_DIM) paint = mix(paint, {0.3f, 0.3f, 0.32f}, 0.5f);
    const Rgb trim = cold ? Rgb{0.25f, 0.22f, 0.18f} : kBrass;
    const Rgb buffer = {0.55f, 0.08f, 0.06f};
    const float s = L.s, z = L.z;
    const bool running = !cold && ((L.state == ST_BUSY && L.action == AC_RUN) || L.speed > 0.02f);
    const float spin = running ? -static_cast<float>(t) * (L.action == AC_RUN ? 5.0f : 3.0f) * L.dir * motion() : L.seed * 6.0f;
    const float bob = running ? 0.02f * s * std::sin(static_cast<float>(t) * 10.0f) * motion() : 0.0f;

    // Chassis and footplate.
    box(ax(L, 0.1f), 0.72f * s, z, 3.9f * s, 0.18f * s, 1.15f * s, kIron);
    box(ax(L, 2.05f), 0.6f * s, z, 0.15f * s, 0.32f * s, 1.25f * s, buffer);
    for (int side = -1; side <= 1; side += 2)
        cyl(ax(L, 2.25f), 0.6f * s, z + side * 0.4f * s, 0.08f * s, 0.12f * s, kIron);
    // Cowcatcher.
    box(ax(L, 2.0f), 0.35f * s, z, 0.35f * s, 0.25f * s, 1.0f * s, scale(kIron, 1.6f));
    // Boiler: a box with a rounded smokebox front and a band of trim.
    box(ax(L, 0.45f), 1.25f * s + bob, z, 2.3f * s, 0.95f * s, 0.95f * s, paint);
    ball(ax(L, 1.6f), 1.25f * s + bob, z, 0.4f * s, 0.95f * s, 0.95f * s, scale(kIron, 1.4f));
    box(ax(L, 0.9f), 1.25f * s + bob, z, 0.08f * s, 0.98f * s, 0.98f * s, trim);
    box(ax(L, -0.1f), 1.25f * s + bob, z, 0.08f * s, 0.98f * s, 0.98f * s, trim);
    // Chimney, dome, whistle.
    cyl(ax(L, 1.2f), 1.9f * s + bob, z, 0.15f * s, 0.25f * s, kIron);
    cyl(ax(L, 1.2f), 2.15f * s + bob, z, 0.21f * s, 0.06f * s, kIron);
    ball(ax(L, 0.35f), 1.78f * s + bob, z, 0.45f * s, 0.4f * s, 0.45f * s, trim);
    // Cab.
    box(ax(L, -1.15f), 1.45f * s, z, 1.05f * s, 1.35f * s, 1.15f * s, scale(paint, 0.85f));
    box(ax(L, -1.15f), 2.18f * s, z, 1.3f * s, 0.1f * s, 1.3f * s, scale(kIron, 1.5f));
    for (int side = -1; side <= 1; side += 2)
        box(ax(L, -1.05f), 1.75f * s, z + side * 0.58f * s, 0.5f * s, 0.35f * s, 0.03f * s,
            cold ? Rgb{0.05f, 0.05f, 0.06f} : Rgb{0.95f, 0.78f, 0.45f});
    // Headlamp: dim when idle, bright when busy, pulsing for "other".
    float lampK = cold ? 0.0f : (L.state == ST_BUSY ? 1.0f : 0.35f);
    if (L.state == ST_BUSY && L.action == AC_OTHER) lampK = 0.4f + 0.6f * (0.5f + 0.5f * std::sin(static_cast<float>(t) * 5.0f));
    const Rgb lamp = mix({0.15f, 0.14f, 0.1f}, {1.0f, 0.92f, 0.6f}, lampK);
    ball(ax(L, 1.85f), 1.65f * s, z, 0.22f * s, 0.22f * s, 0.22f * s, lamp);
    if (lampK > 0.1f) glow(ax(L, 1.98f), 1.65f * s, z, 8.0f, 0.7f * s * lampK, {1.0f, 0.92f, 0.6f}, 0.8f * lampK);
    // Driving wheels with turning spokes, and a side rod between the crank pins.
    for (int side = -1; side <= 1; side += 2) {
        const float zs = side * 0.6f;
        for (int k = 0; k < 3; ++k) wheel(L, 0.95f - k * 0.8f, zs, 0.36f, spin + k * 0.0f, {0.55f, 0.08f, 0.06f});
        const float cx = std::cos(spin) * 0.18f * s, cy = std::sin(spin) * 0.18f * s;
        const float zr = z + zs * s + side * 0.1f * s;
        wline(ax(L, 0.95f) + cx, 0.56f * s + 0.2f + cy, zr, ax(L, -0.65f) + cx, 0.56f * s + 0.2f + cy, zr,
              {0.75f, 0.75f, 0.78f}, 2.0f, 0.06f * s);
    }

    // Tender with coal.
    const float tx = -2.5f;
    box(ax(L, tx), 1.15f * s, z, 1.5f * s, 1.0f * s, 1.15f * s, scale(paint, 0.9f));
    box(ax(L, tx), 1.7f * s, z, 1.3f * s, 0.15f * s, 1.0f * s, {0.03f, 0.03f, 0.03f});
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 2; ++k) wheel(L, tx - 0.4f + k * 0.8f, side * 0.6f, 0.25f, spin * 1.4f, kIron);

    // A flat wagon behind: freight crates load onto it while editing.
    const float wx = -4.6f;
    box(ax(L, wx), 0.72f * s, z, 2.3f * s, 0.15f * s, 1.15f * s, {0.32f, 0.22f, 0.13f});
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 2; ++k) wheel(L, wx - 0.7f + k * 1.4f, side * 0.6f, 0.25f, spin * 1.4f, kIron);
    const Rgb crate{0.62f, 0.44f, 0.22f};
    if (!cold && L.state == ST_BUSY && L.action == AC_EDIT) {
        // A crane arm over the wagon lowers crate after crate; the wagon fills
        // and empties in a loop.
        const float cyc = motion() > 0 ? fract(static_cast<float>(t) * 0.35f + L.seed) * 4.0f : 2.5f;
        const int loaded = static_cast<int>(cyc);
        const float drop = cyc - loaded;
        const float slots[4][2] = {{-0.5f, -0.25f}, {0.5f, -0.25f}, {-0.5f, 0.25f}, {0.5f, 0.25f}};
        for (int k = 0; k < loaded; ++k)
            box(ax(L, wx + slots[k][0]), 1.05f * s, z + slots[k][1] * s, 0.8f * s, 0.5f * s, 0.45f * s,
                k % 2 ? crate : scale(crate, 0.8f));
        if (loaded < 4) {
            const float cxk = ax(L, wx + slots[loaded][0]), czk = z + slots[loaded][1] * s;
            const float cy = (1.05f + (1.0f - drop) * 1.8f) * s;
            box(cxk, cy, czk, 0.8f * s, 0.5f * s, 0.45f * s, crate);
            host().line(cxk, cy + 0.25f * s, czk, cxk, 3.4f * s, czk, kIron, 1.0f);
        }
        // Crane: mast and jib on the +z side.
        box(ax(L, wx), 1.7f * s, z + 1.15f * s, 0.15f * s, 3.4f * s, 0.15f * s, {0.85f, 0.65f, 0.1f});
        box(ax(L, wx), 3.4f * s, z + 0.55f * s, 0.12f * s, 0.12f * s, 1.3f * s, {0.85f, 0.65f, 0.1f});
    } else if (!cold) {
        box(ax(L, wx - 0.5f), 1.0f * s, z, 0.8f * s, 0.4f * s, 0.6f * s, scale(crate, 0.7f));
    }

    // Selection ring on the ballast.
    if (L.flags & (FL_SELECTED | FL_HOVER)) {
        const bool sel = (L.flags & FL_SELECTED) != 0;
        const float cx = ax(L, -1.3f), rx = 4.4f * s, rz = 1.3f * s;
        for (int i = 0; i < 40; ++i) {
            const float a0 = i * 2 * kPi / 40, a1 = (i + 1) * 2 * kPi / 40;
            wline(cx + std::cos(a0) * rx, 0.3f, z + std::sin(a0) * rz, cx + std::cos(a1) * rx, 0.3f,
                  z + std::sin(a1) * rz, sel ? Rgb{1, 1, 1} : Rgb{0.7f, 0.8f, 1.0f}, sel ? 3.0f : 2.0f, 0.0f,
                  sel ? 0.95f : 0.5f);
        }
    }

    // Signal and its state.
    Rgb sig;
    bool lit;
    buildSignal(L, t, sig, lit);
    if (cold) return;

    // Smoke and steam.
    if (stalled) {
        smoke(L, t, {0.85f, 0.08f, 0.05f}, 12, 0.5f, 3.2f, 1.1f, 0.85f);
    } else if (L.state == ST_BUSY) {
        switch (L.action) {
            case AC_RUN: smoke(L, t, {0.16f, 0.16f, 0.18f}, 16, 0.9f, 4.5f, 1.7f, 0.95f); break;
            case AC_THINK: smoke(L, t, {0.95f, 0.95f, 0.98f}, 5, 0.22f, 2.2f, 1.2f, 0.55f); break;
            default: smoke(L, t, {0.75f, 0.75f, 0.78f}, 5, 0.45f, 2.0f, 0.6f, 0.35f); break;
        }
    } else if (L.state == ST_WAITING) {
        smoke(L, t, {0.95f, 0.95f, 0.98f}, 3, 0.2f, 1.2f, 0.6f, 0.35f);
    }
    if (L.state != ST_BUSY || stalled) return;

    const float ft = static_cast<float>(t);
    if (L.action == AC_THINK) {
        // Idle steam: soft puffs from the cylinders by the front wheels too.
        for (int side = -1; side <= 1; side += 2)
            for (int i = 0; i < 3; ++i) {
                const float age = fract(ft * 0.3f + i / 3.0f + L.seed + side * 0.17f);
                glow(ax(L, 1.4f) - L.dir * age * 0.5f * s, (0.4f + age * 0.6f) * s, z + side * (0.75f + age * 0.8f) * s,
                     4.0f, (0.3f + age * 0.8f) * s, {0.95f, 0.95f, 1.0f}, 0.5f * (1.0f - age));
            }
    } else if (L.action == AC_READ) {
        // The inspector walks the length of the train with a lantern, on the
        // +z side, there and back.
        const float span = 6.8f;
        const float c = motion() > 0 ? fract(ft * 0.07f + L.seed) : 0.3f;
        const float u = c < 0.5f ? c * 2.0f : 2.0f - c * 2.0f;
        const float off = 1.9f - u * span;
        const float px = ax(L, off), pz = z + 1.0f * s;
        const float step = std::fabs(std::sin(ft * 7.0f)) * 0.06f * s * motion();
        host().add(STRATA_PRIMITIVE_CAPSULE, px, 0.75f * s + step, pz, 0.32f * s, 0.32f * s, 0.32f * s,
                   {0.12f, 0.14f, 0.25f});
        ball(px, 1.22f * s + step, pz, 0.26f * s, 0.26f * s, 0.26f * s, {0.85f, 0.68f, 0.55f});
        box(px, 1.38f * s + step, pz, 0.34f * s, 0.08f * s, 0.34f * s, {0.06f, 0.06f, 0.1f});
        const float lx = px + (c < 0.5f ? -0.25f : 0.25f) * L.dir * s, ly = 0.85f * s + step, lz = pz - 0.1f * s;
        ball(lx, ly, lz, 0.16f * s, 0.2f * s, 0.16f * s, {1.0f, 0.85f, 0.35f});
        glow(lx, ly, lz, 12.0f, 0.9f * s, {1.0f, 0.8f, 0.35f}, 0.85f);
        host().light(lx, ly + 0.2f, lz, 3.5f, {1.0f, 0.75f, 0.35f}, 1.6f, 3);
    } else if (L.action == AC_WEB) {
        // A telegraph pole on the +z side, wires running off, sparks at the
        // insulators and pulses running down the wire.
        const float px = ax(L, 1.0f), pz = z + 1.25f * s, ph = 3.2f * s;
        const float qx = ax(L, -6.5f);
        for (float xx : {px, qx}) {
            cyl(xx, ph * 0.5f, pz, 0.07f * s, ph * 0.5f, {0.35f, 0.24f, 0.14f});
            box(xx, ph - 0.25f * s, pz, 0.12f * s, 0.1f * s, 1.0f * s, {0.35f, 0.24f, 0.14f});
        }
        for (int w = -1; w <= 1; w += 2) {
            const float wz = pz + w * 0.4f * s, wy = ph - 0.15f * s;
            for (int k = 0; k < 8; ++k) {
                const float u0 = k / 8.0f, u1 = (k + 1) / 8.0f;
                const float sag0 = 4.0f * u0 * (1 - u0) * 0.3f * s, sag1 = 4.0f * u1 * (1 - u1) * 0.3f * s;
                host().line(px + (qx - px) * u0, wy - sag0, wz, px + (qx - px) * u1, wy - sag1, wz, kIron, 0.9f);
            }
            ball(px, wy, wz, 0.1f * s, 0.12f * s, 0.1f * s, {0.7f, 0.9f, 0.85f});
            const float u = fract(ft * 0.9f + (w > 0 ? 0.5f : 0.0f));
            glow(px + (qx - px) * u, wy - 4.0f * u * (1 - u) * 0.3f * s, wz, 8.0f, 0.3f * s, {0.6f, 0.85f, 1.0f}, 0.95f);
        }
        for (int i = 0; i < 6; ++i) {
            const float k = std::floor(ft * 12.0f) + i * 7.0f;
            const float hx = hash(L.seed + k, 1.0f), hz = hash(L.seed + k, 2.0f), hy = hash(L.seed + k, 3.0f);
            if (hash(k, L.seed) < 0.55f)
                glow(px + (hx - 0.5f) * 0.6f * s, ph + hy * 0.6f * s, pz + (hz - 0.5f) * 1.0f * s, 5.0f, 0.15f * s,
                     {0.75f, 0.9f, 1.0f}, 1.0f);
        }
        host().light(px, ph + 0.3f, pz, 3.5f, {0.6f, 0.8f, 1.0f}, 1.2f + hash(std::floor(ft * 12.0f), L.seed), 3);
    } else if (L.action == AC_OTHER) {
        // Generic busy: a whistle of steam from the cab roof, on and off.
        const bool on = fract(ft * 0.5f + L.seed) < 0.35f;
        if (on)
            for (int i = 0; i < 6; ++i) {
                const float age = fract(ft * 1.5f + i / 6.0f);
                glow(ax(L, 0.9f), (1.9f + age * 1.4f) * s, z, 4.0f, (0.15f + age * 0.5f) * s, {1.0f, 1.0f, 1.0f},
                     0.7f * (1.0f - age));
            }
        ball(ax(L, 0.9f), 1.85f * s, z, 0.1f * s, 0.3f * s, 0.1f * s, kBrass);
    }
}

}  // namespace

void strata_kit_configure(strata_kit::Options& o) {
    o.name = "agentviz-railway";
    o.farPlane = 600.0f;
}

void strata_kit_engine_ready(strata_kit::Host&) {}

void strata_kit_build(strata_kit::Host& h, double t) {
    h.ambientTint = {1.0f, 0.97f, 0.92f};
    // Ground and props first (they size the yard), then rails, then trains.
    for (size_t i = 0; i + kPropStride <= g.props.size(); i += kPropStride) {
        const float* p = &g.props[i];
        switch (static_cast<int>(p[0])) {
            case PR_GROUND: {
                const float x0 = p[1], z0 = p[2], x1 = p[3], z1 = p[4];
                box((x0 + x1) * 0.5f, -0.1f, (z0 + z1) * 0.5f, x1 - x0, 0.2f, z1 - z0, {0.30f, 0.36f, 0.20f});
                break;
            }
            case PR_SHED: buildShed(p[1], p[3], p[2], p[4]); break;
            case PR_SWITCH: buildSwitch(p, t); break;
            case PR_BUFFER: buildBuffer(p[1], p[2], p[3]); break;
            case PR_STATION: buildStation(p[1], p[2], p[3]); break;
            case PR_GLOW: {
                const Rgb c = p[3] > 0.5f ? kSignalGreen : Rgb{1.0f, 0.95f, 0.7f};
                glow(p[1], 0.45f, p[2], 10.0f, 0.6f, c, std::clamp(p[4], 0.0f, 1.0f));
                break;
            }
            default: break;
        }
    }
    for (size_t i = 0; i + kRailStride <= g.rails.size(); i += kRailStride) buildRail(&g.rails[i]);
    for (size_t i = 0; i + kTrainStride <= g.trains.size(); i += kTrainStride) {
        const float* r = &g.trains[i];
        Loco L{r[0], r[1], r[2], r[3] < 0 ? -1.0f : 1.0f, static_cast<int>(r[4]), static_cast<int>(r[5]), r[6],
               static_cast<int>(r[7]), r[8], r[9] > 0.5f};
        buildLoco(L, t);
    }
    // A low warm fill so shaded sides of the yard keep their colour.
    StrataPointLight fillLight{};
    fillLight.position[0] = 0;
    fillLight.position[1] = 40;
    fillLight.position[2] = 0;
    fillLight.range = 1.0f;
    fillLight.color[0] = fillLight.color[1] = fillLight.color[2] = 1.0f;
    fillLight.intensity = 0.0f;
    h.fill(fillLight, {0.06f, 0.055f, 0.05f}, -1);
}

// The page's setters: each copies a flat float array (see the header).
STRATA_KIT_EXPORT void rw_set_rails(const float* rows, int count) { g.rails.assign(rows, rows + count * kRailStride); }
STRATA_KIT_EXPORT void rw_set_trains(const float* rows, int count) {
    g.trains.assign(rows, rows + count * kTrainStride);
}
STRATA_KIT_EXPORT void rw_set_props(const float* rows, int count) { g.props.assign(rows, rows + count * kPropStride); }
