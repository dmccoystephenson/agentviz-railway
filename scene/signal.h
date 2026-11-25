// signal.h: each train's colour-light signal (green busy, amber idle or
// resting, flashing red waiting, dark offline) and, for a train waiting on
// the owner, the bell, the reason board, the red beam and the ground ring.
#pragma once

#include "loco.h"

namespace railway {

struct Lamp {
    Rgb colour{0.04f, 0.04f, 0.04f};
    float alpha = 0.85f;
    bool lit = true;
};

inline Lamp lampOf(const Loco& L, float t) {
    Lamp lamp;
    switch (L.state) {
        case State::Busy: lamp.colour = kSignalGreen; break;
        case State::Idle: lamp.colour = kSignalAmber; break;
        case State::Waiting:
            lamp.lit = motion() == 0 || fract(t * 2.0f) < 0.6f;
            lamp.colour = lamp.lit ? kSignalRed : Rgb{0.15f, 0.02f, 0.02f};
            lamp.alpha = 1.0f;
            break;
        case State::Dim: lamp.colour = {0.35f, 0.42f, 0.55f}; lamp.alpha = 0.4f; break;
        default: lamp.lit = false; break;
    }
    return lamp;
}

// The bell on a bracket above the signal, swinging, rings going out each strike.
inline void drawBell(const Loco& L, float sx, float sz, float by, float t) {
    const float s = L.s, swing = motion() * std::sin(t * 9.0f);
    box(sx, by - 0.4f * s, sz, 0.08f * s, 0.5f * s, 0.08f * s, kIron);
    box(sx, by + 0.35f * s, sz, 0.7f * s, 0.08f * s, 0.08f * s, kIron);
    ball(sx + swing * 0.18f * s, by, sz, 0.5f * s, 0.55f * s, 0.5f * s, kBrass);
    ball(sx + swing * 0.3f * s, by - 0.3f * s, sz, 0.14f * s, 0.14f * s, 0.14f * s, kIron);
    for (int k = 0; k < 3; ++k) {
        const float age = fract(t * 1.4f + k / 3.0f);
        const float r = (0.4f + age * 2.4f) * s;
        ring(sx, by, sz, r, r, kBrass, (1.0f - age) * 0.9f);
    }
}

// Why it waits, on a white board above the bell, facing the yard: a red "?"
// for a question, an iron padlock for a permission prompt.
inline void drawReasonBoard(const Loco& L, float x, float y, float z) {
    if (L.reason == agentviz::WaitingReason::None) return;
    const float s = L.s, front = z + 0.07f * s;
    box(x, y - 0.55f * s, z, 0.06f * s, 0.5f * s, 0.06f * s, kIron);
    ball(x, y, z, 0.95f * s, 0.95f * s, 0.08f * s, kWhite);
    glow(x, y, z, 14.0f, 0.6f * s, kWhite, 0.35f);
    if (L.reason == agentviz::WaitingReason::Question) {
        const Rgb ink{0.8f, 0.05f, 0.03f};
        const float cx = x, cy = y + 0.12f * s, r = 0.17f * s;
        for (int i = 0; i < 8; ++i) {  // the hook: from upper left over the top and down the right
            const float a0 = 2.6f - i * 0.45f, a1 = 2.6f - (i + 1) * 0.45f;
            wline(cx + std::cos(a0) * r, cy + std::sin(a0) * r, front, cx + std::cos(a1) * r, cy + std::sin(a1) * r,
                  front, ink, 3.0f, 0.06f * s);
        }
        wline(cx, cy - r * 0.95f, front, cx, cy - 0.27f * s, front, ink, 3.0f, 0.06f * s);
        ball(cx, y - 0.28f * s, front, 0.1f * s, 0.1f * s, 0.05f * s, ink);
    } else {
        const float top = y + 0.0f * s, r = 0.13f * s;
        for (int i = 0; i < 8; ++i) {  // the shackle: a half circle over the body
            const float a0 = i * kPi / 8, a1 = (i + 1) * kPi / 8;
            wline(x + std::cos(a0) * r, top + std::sin(a0) * r, front, x + std::cos(a1) * r, top + std::sin(a1) * r,
                  front, scale(kIron, 3.0f), 3.0f, 0.05f * s);
        }
        box(x, top - 0.15f * s, front, 0.42f * s, 0.32f * s, 0.06f * s, scale(kIron, 2.0f));
        ball(x, top - 0.13f * s, front + 0.03f * s, 0.08f * s, 0.08f * s, 0.04f * s, kBrass);
    }
}

// A tall pulsing red beam over the train and a turning ring on the ground, so
// a waiting train can be found from anywhere in the yard.
inline void drawWaitBeacon(const Loco& L, float t) {
    const float pulse = 0.55f + 0.45f * std::sin(t * 6.0f) * motion();
    wline(L.x, 0.3f, L.z, L.x, 14.0f, L.z, kRedBeam, 6.0f, 0.5f, 0.35f + 0.35f * pulse);
    glow(L.x, 7.0f, L.z, 30.0f, 3.0f, kSignalRed, 0.25f * pulse);
    const float r = 4.2f * L.s * (0.92f + 0.08f * pulse);
    wideRing(L.x, 0.3f, L.z, r, r, kSignalRed, 4.0f, 0.18f, 0.9f, 32, t * motion());
    light(L.x, 3.0f, L.z, 9.0f, kSignalRed, 2.5f * pulse, 10);
}

// A colour-light signal on the far (-z) side, by the front of the engine.
inline void drawSignal(const Loco& L, float t) {
    const float s = L.s, sx = L.at(2.3f), sz = L.z - 1.15f * s, h = (L.lead ? 2.6f : 2.0f) * s;
    const bool waiting = L.state == State::Waiting;
    cyl(sx, h * 0.5f, sz, 0.06f * s, h * 0.5f, kIron);
    box(sx, h, sz, 0.42f * s, 0.7f * s, 0.3f * s, kIron);
    const Lamp lamp = lampOf(L, t);
    for (int side = -1; side <= 1; side += 2)
        ball(sx, h, sz + side * 0.16f * s, 0.28f * s, 0.28f * s, 0.08f * s, lamp.colour);
    if (lamp.lit) {
        glow(sx, h, sz + 0.2f * s, waiting ? 20.0f : 10.0f, 0.9f * s, lamp.colour, lamp.alpha);
        light(sx, h + 0.2f, sz, 4.0f * s, lamp.colour, 1.0f, waiting ? 9 : 2);
    }
    if (!waiting) return;
    const float bellY = h + 0.95f * s;
    drawBell(L, sx, sz, bellY, t);
    drawReasonBoard(L, sx, bellY + 1.15f * s, sz);
    drawWaitBeacon(L, t);
}

}  // namespace railway
