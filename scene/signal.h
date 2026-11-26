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

// Why it waits, on a square white board with a red frame high over the bell,
// big enough to read from across the yard: a red "?" for a question, an iron
// padlock for a permission prompt. The sign is painted on both faces.
inline void drawQuestionMark(float x, float y, float face, float s) {
    const Rgb ink{0.85f, 0.04f, 0.02f};
    const float cy = y + 0.2f * s, r = 0.3f * s, w = 0.11f * s;
    for (int i = 0; i < 9; ++i) {  // the hook: from upper left over the top and down the right
        const float a0 = 2.7f - i * 0.42f, a1 = 2.7f - (i + 1) * 0.42f;
        wline(x + std::cos(a0) * r, cy + std::sin(a0) * r, face, x + std::cos(a1) * r, cy + std::sin(a1) * r, face,
              ink, 8.0f, w);
    }
    wline(x, cy - r * 0.95f, face, x, y - 0.22f * s, face, ink, 8.0f, w);
    ball(x, y - 0.46f * s, face, 0.17f * s, 0.17f * s, 0.06f * s, ink);
}

inline void drawPadlock(float x, float y, float face, float out, float s) {
    const Rgb iron{0.06f, 0.06f, 0.07f};
    const float top = y + 0.08f * s, r = 0.24f * s;
    for (int i = 0; i < 10; ++i) {  // the shackle: a half circle over the body, with short legs
        const float a0 = i * kPi / 10, a1 = (i + 1) * kPi / 10;
        wline(x + std::cos(a0) * r, top + std::sin(a0) * r, face, x + std::cos(a1) * r, top + std::sin(a1) * r, face,
              iron, 8.0f, 0.1f * s);
    }
    for (int side = -1; side <= 1; side += 2)
        wline(x + side * r, top, face, x + side * r, top - 0.12f * s, face, iron, 8.0f, 0.1f * s);
    box(x, y - 0.28f * s, face, 0.78f * s, 0.6f * s, 0.06f * s, iron);
    ball(x, y - 0.22f * s, face + out * 0.04f * s, 0.14f * s, 0.14f * s, 0.04f * s, kBrass);
    box(x, y - 0.36f * s, face + out * 0.04f * s, 0.06f * s, 0.18f * s, 0.04f * s, kBrass);
}

inline void drawReasonBoard(const Loco& L, float x, float y, float z) {
    if (L.reason == agentviz::WaitingReason::None) return;
    const float s = L.s, half = 0.08f * s;
    box(x, y - 1.05f * s, z, 0.08f * s, 0.9f * s, 0.08f * s, kIron);           // the post down to the bell bracket
    box(x, y, z, 1.75f * s, 1.75f * s, 0.1f * s, kSignalRed);                    // the frame
    box(x, y, z, 1.5f * s, 1.5f * s, 0.14f * s, kWhite);                         // the face
    glow(x, y, z, 26.0f, 1.3f * s, kWhite, 0.45f);
    for (int side = -1; side <= 1; side += 2) {
        const float face = z + side * half;
        if (L.reason == agentviz::WaitingReason::Question) drawQuestionMark(x, y, face, s);
        else drawPadlock(x, y, face, float(side), s);
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
    drawReasonBoard(L, sx, bellY + 1.75f * s, sz);
    drawWaitBeacon(L, t);
}

}  // namespace railway
