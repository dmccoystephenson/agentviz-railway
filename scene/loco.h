// loco.h: one locomotive per session (a big engine on its trunk) or subagent
// (a smaller one on its siding): engine, tender and a flat wagon, decoded
// from an agentviz::Agent. Offsets along the train are multiplied by `dir`
// (1 faces +x down the line, -1 faces back towards the trunk).
#pragma once

#include <algorithm>

#include "../vendor/agentviz-kit/scene/agentviz.h"
#include "draw_util.h"

namespace railway {

using agentviz::Action;
using agentviz::State;

// Agent extras (web/railway-frame.js).
constexpr int AGENT_SCALE = 0, AGENT_SPEED = 1;

struct Loco {
    float x, z, s, dir, seed, speed, pulse;
    State state;
    Action action;
    agentviz::WaitingReason reason;
    agentviz::Highlight highlight;
    bool stalled, lead;

    bool cold() const { return state == State::Offline; }
    bool busy(Action a) const { return state == State::Busy && action == a; }
    // Wheels turn while it steams (run) or rolls along its siding.
    bool running() const { return !cold() && (busy(Action::Run) || speed > 0.02f); }
    float at(float off) const { return x + off * s * dir; }
};

inline Loco locoOf(const agentviz::Agent& a) {
    const float s = a.extra[AGENT_SCALE] > 0.0f ? a.extra[AGENT_SCALE] : 1.0f;
    return {a.x, a.z, s, std::cos(a.facing) < 0.0f ? -1.0f : 1.0f, a.seed, a.extra[AGENT_SPEED], a.pulse,
            a.state, a.action, a.waitingReason, a.highlight, a.stalled, a.lead()};
}

// Liveries, chosen by seed: deep green, maroon, blue, black, teal, ochre.
inline Rgb liveryOf(const Loco& L) {
    static const Rgb kLivery[6] = {{0.08f, 0.32f, 0.16f}, {0.42f, 0.08f, 0.08f}, {0.10f, 0.20f, 0.48f},
                                   {0.14f, 0.14f, 0.16f}, {0.05f, 0.36f, 0.38f}, {0.55f, 0.38f, 0.08f}};
    Rgb paint = kLivery[std::clamp(static_cast<int>(L.seed * 6.0f), 0, 5)];
    if (L.cold()) paint = mix(scale(paint, 0.5f), {0.18f, 0.18f, 0.2f}, 0.6f);
    if (L.state == State::Dim) paint = mix(paint, {0.3f, 0.3f, 0.32f}, 0.5f);
    return paint;
}

inline void wheel(const Loco& L, float off, float zside, float r, float spin, Rgb rim) {
    const float x = L.at(off), y = r * L.s + 0.2f, z = L.z + zside * L.s, out = zside > 0 ? 1.0f : -1.0f;
    ball(x, y, z, 2 * r * L.s, 2 * r * L.s, 0.1f * L.s, rim);
    ball(x, y, z + out * 0.04f * L.s, 0.45f * r * L.s, 0.45f * r * L.s, 0.1f * L.s, kIron);
    const float zs = z + out * 0.07f * L.s;
    for (int k = 0; k < 3; ++k) {
        const float a = spin + k * kPi / 3.0f;
        const float ex = std::cos(a) * r * 0.9f * L.s, ey = std::sin(a) * r * 0.9f * L.s;
        line(x - ex, y - ey, zs, x + ex, y + ey, zs, {0.9f, 0.85f, 0.7f}, 0.9f);
    }
}

// Chassis, front buffer beam with buffers, and the cowcatcher.
inline void drawChassis(const Loco& L) {
    const float s = L.s, z = L.z;
    box(L.at(0.1f), 0.72f * s, z, 3.9f * s, 0.18f * s, 1.15f * s, kIron);
    box(L.at(2.05f), 0.6f * s, z, 0.15f * s, 0.32f * s, 1.25f * s, kBufferRed);
    for (int side = -1; side <= 1; side += 2) cyl(L.at(2.25f), 0.6f * s, z + side * 0.4f * s, 0.08f * s, 0.12f * s, kIron);
    box(L.at(2.0f), 0.35f * s, z, 0.35f * s, 0.25f * s, 1.0f * s, scale(kIron, 1.6f));
}

// Boiler with a rounded smokebox front and trim bands; chimney and dome.
inline void drawBoiler(const Loco& L, Rgb paint, Rgb trim, float bob) {
    const float s = L.s, z = L.z, y = 1.25f * s + bob;
    box(L.at(0.45f), y, z, 2.3f * s, 0.95f * s, 0.95f * s, paint);
    ball(L.at(1.6f), y, z, 0.4f * s, 0.95f * s, 0.95f * s, scale(kIron, 1.4f));
    box(L.at(0.9f), y, z, 0.08f * s, 0.98f * s, 0.98f * s, trim);
    box(L.at(-0.1f), y, z, 0.08f * s, 0.98f * s, 0.98f * s, trim);
    cyl(L.at(1.2f), 1.9f * s + bob, z, 0.15f * s, 0.25f * s, kIron);
    cyl(L.at(1.2f), 2.15f * s + bob, z, 0.21f * s, 0.06f * s, kIron);
    ball(L.at(0.35f), 1.78f * s + bob, z, 0.45f * s, 0.4f * s, 0.45f * s, trim);
}

// The cab, its roof and lit windows (dark when the engine is cold).
inline void drawCab(const Loco& L, Rgb paint) {
    const float s = L.s, z = L.z;
    box(L.at(-1.15f), 1.45f * s, z, 1.05f * s, 1.35f * s, 1.15f * s, scale(paint, 0.85f));
    box(L.at(-1.15f), 2.18f * s, z, 1.3f * s, 0.1f * s, 1.3f * s, scale(kIron, 1.5f));
    const Rgb window = L.cold() ? Rgb{0.05f, 0.05f, 0.06f} : Rgb{0.95f, 0.78f, 0.45f};
    for (int side = -1; side <= 1; side += 2)
        box(L.at(-1.05f), 1.75f * s, z + side * 0.58f * s, 0.5f * s, 0.35f * s, 0.03f * s, window);
}

// Headlamp: dim when idle, bright when busy, pulsing for "other", and a
// flash at each new tool call.
inline void drawHeadlamp(const Loco& L, float t) {
    float k = L.cold() ? 0.0f : (L.state == State::Busy ? 1.0f : 0.35f);
    if (L.busy(Action::Other)) k = 0.4f + 0.6f * (0.5f + 0.5f * std::sin(t * 5.0f));
    if (!L.cold()) k = std::max(k, L.pulse);
    const Rgb warm{1.0f, 0.92f, 0.6f};
    ball(L.at(1.85f), 1.65f * L.s, L.z, 0.22f * L.s, 0.22f * L.s, 0.22f * L.s, mix({0.15f, 0.14f, 0.1f}, warm, k));
    if (k > 0.1f) glow(L.at(1.98f), 1.65f * L.s, L.z, 8.0f, 0.7f * L.s * k * (1.0f + L.pulse), warm, 0.8f * k);
}

// Three driving wheels a side with turning spokes, and a side rod between the crank pins.
inline void drawDrivingWheels(const Loco& L, float spin) {
    for (int side = -1; side <= 1; side += 2) {
        const float zs = side * 0.6f;
        for (int k = 0; k < 3; ++k) wheel(L, 0.95f - k * 0.8f, zs, 0.36f, spin, kBufferRed);
        const float cx = std::cos(spin) * 0.18f * L.s, cy = 0.56f * L.s + 0.2f + std::sin(spin) * 0.18f * L.s;
        const float zr = L.z + (zs + side * 0.1f) * L.s;
        wline(L.at(0.95f) + cx, cy, zr, L.at(-0.65f) + cx, cy, zr, {0.75f, 0.75f, 0.78f}, 2.0f, 0.06f * L.s);
    }
}

inline void drawTender(const Loco& L, Rgb paint, float spin) {
    const float s = L.s, z = L.z, tx = -2.5f;
    box(L.at(tx), 1.15f * s, z, 1.5f * s, 1.0f * s, 1.15f * s, scale(paint, 0.9f));
    box(L.at(tx), 1.7f * s, z, 1.3f * s, 0.15f * s, 1.0f * s, {0.03f, 0.03f, 0.03f});
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 2; ++k) wheel(L, tx - 0.4f + k * 0.8f, side * 0.6f, 0.25f, spin * 1.4f, kIron);
}

constexpr float kWagonX = -4.6f;
const Rgb kCrate{0.62f, 0.44f, 0.22f};

// Editing: a crane over the wagon lowers crate after crate; the wagon fills
// and empties in a loop.
inline void drawCraneLoading(const Loco& L, float t) {
    const float s = L.s, z = L.z;
    const float cycle = motion() > 0 ? fract(t * 0.35f + L.seed) * 4.0f : 2.5f;
    const int loaded = static_cast<int>(cycle);
    const float slots[4][2] = {{-0.5f, -0.25f}, {0.5f, -0.25f}, {-0.5f, 0.25f}, {0.5f, 0.25f}};
    for (int k = 0; k < loaded; ++k)
        box(L.at(kWagonX + slots[k][0]), 1.05f * s, z + slots[k][1] * s, 0.8f * s, 0.5f * s, 0.45f * s,
            k % 2 ? kCrate : scale(kCrate, 0.8f));
    if (loaded < 4) {
        const float cx = L.at(kWagonX + slots[loaded][0]), cz = z + slots[loaded][1] * s;
        const float cy = (1.05f + (1.0f - (cycle - loaded)) * 1.8f) * s;
        box(cx, cy, cz, 0.8f * s, 0.5f * s, 0.45f * s, kCrate);
        line(cx, cy + 0.25f * s, cz, cx, 3.4f * s, cz, kIron, 1.0f);
    }
    const Rgb yellow{0.85f, 0.65f, 0.1f};
    box(L.at(kWagonX), 1.7f * s, z + 1.15f * s, 0.15f * s, 3.4f * s, 0.15f * s, yellow);
    box(L.at(kWagonX), 3.4f * s, z + 0.55f * s, 0.12f * s, 0.12f * s, 1.3f * s, yellow);
}

// A flat wagon behind the tender; one crate on it unless it is loading.
inline void drawWagon(const Loco& L, float t, float spin) {
    const float s = L.s, z = L.z;
    box(L.at(kWagonX), 0.72f * s, z, 2.3f * s, 0.15f * s, 1.15f * s, {0.32f, 0.22f, 0.13f});
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 2; ++k) wheel(L, kWagonX - 0.7f + k * 1.4f, side * 0.6f, 0.25f, spin * 1.4f, kIron);
    if (L.cold()) return;
    if (L.busy(Action::Edit)) drawCraneLoading(L, t);
    else box(L.at(kWagonX - 0.5f), 1.0f * s, z, 0.8f * s, 0.4f * s, 0.6f * s, scale(kCrate, 0.7f));
}

// A white ring on the ballast round the selected train; a faint blue one on hover.
inline void drawSelection(const Loco& L) {
    if (L.highlight == agentviz::Highlight::None) return;
    const bool selected = L.highlight == agentviz::Highlight::Selected;
    wideRing(L.at(-1.3f), 0.3f, L.z, 4.4f * L.s, 1.3f * L.s, selected ? kWhite : Rgb{0.7f, 0.8f, 1.0f},
             selected ? 3.0f : 2.0f, 0.0f, selected ? 0.95f : 0.5f, 40);
}

// The engine, tender and wagon. Signals and smoke are drawn by the caller.
inline void drawTrain(const Loco& L, float t) {
    const Rgb paint = liveryOf(L), trim = L.cold() ? Rgb{0.25f, 0.22f, 0.18f} : kBrass;
    const float wheelRate = L.busy(Action::Run) ? 5.0f : 3.0f;
    const float spin = L.running() ? -t * wheelRate * L.dir * motion() : L.seed * 6.0f;
    const float bob = L.running() ? 0.02f * L.s * std::sin(t * 10.0f) * motion() : 0.0f;
    drawChassis(L);
    drawBoiler(L, paint, trim, bob);
    drawCab(L, paint);
    drawHeadlamp(L, t);
    drawDrivingWheels(L, spin);
    drawTender(L, paint, spin);
    drawWagon(L, t, spin);
    drawSelection(L);
}

}  // namespace railway
