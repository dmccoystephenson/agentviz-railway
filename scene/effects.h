// effects.h: what a train is doing, drawn around it: smoke and steam, the
// crane (in loco.h), the inspector's lantern, the telegraph, the whistle,
// and a whistle blast at each new tool call. Every animation comes from scene
// time, so ?drive=1 captures repeat.
#pragma once

#include "loco.h"

namespace railway {

// Puffs from the chimney, drifting back as they rise (further when moving).
inline void smoke(const Loco& L, float t, Rgb c, int n, float rate, float rise, float size, float alpha) {
    const float cx = L.at(1.2f), cy = 2.15f * L.s;
    for (int i = 0; i < n; ++i) {
        const float offset = i / static_cast<float>(n);
        const float age = motion() > 0 ? fract(t * rate + offset + L.seed) : (i + 0.5f) / n;
        const float jitter = hash(L.seed * 31.0f + i, std::floor(t * rate + offset));
        const float x = cx - L.dir * age * (1.2f + L.speed * 2.0f) * L.s + (jitter - 0.5f) * 0.4f * L.s;
        const float z = L.z + (hash(jitter, 3.0f) - 0.5f) * 0.6f * L.s * age;
        glow(x, cy + age * rise * L.s, z, 4.0f, (0.35f + age * size) * L.s, c, alpha * (1.0f - age));
    }
}

// Red smoke when stalled; dark and thick when running a command; slow white
// puffs when thinking; a light wisp otherwise; a thin one while waiting.
inline void drawSmoke(const Loco& L, float t) {
    if (L.stalled) return smoke(L, t, {0.85f, 0.08f, 0.05f}, 12, 0.5f, 3.2f, 1.1f, 0.85f);
    if (L.state == State::Waiting) return smoke(L, t, kSteam, 3, 0.2f, 1.2f, 0.6f, 0.35f);
    if (L.state != State::Busy) return;
    switch (L.action) {
        case Action::Run: return smoke(L, t, {0.16f, 0.16f, 0.18f}, 16, 0.9f, 4.5f, 1.7f, 0.95f);
        case Action::Think: return smoke(L, t, kSteam, 5, 0.22f, 2.2f, 1.2f, 0.55f);
        default: return smoke(L, t, {0.75f, 0.75f, 0.78f}, 5, 0.45f, 2.0f, 0.6f, 0.35f);
    }
}

// Thinking: soft steam from the cylinders by the front wheels too.
inline void drawCylinderSteam(const Loco& L, float t) {
    for (int side = -1; side <= 1; side += 2)
        for (int i = 0; i < 3; ++i) {
            const float age = fract(t * 0.3f + i / 3.0f + L.seed + side * 0.17f);
            glow(L.at(1.4f) - L.dir * age * 0.5f * L.s, (0.4f + age * 0.6f) * L.s, L.z + side * (0.75f + age * 0.8f) * L.s,
                 4.0f, (0.3f + age * 0.8f) * L.s, {0.95f, 0.95f, 1.0f}, 0.5f * (1.0f - age));
        }
}

// Reading: the inspector walks the length of the train with a lantern on the
// near (+z) side, there and back.
inline void drawInspector(const Loco& L, float t) {
    const float s = L.s, span = 6.8f;
    const float c = motion() > 0 ? fract(t * 0.07f + L.seed) : 0.3f;
    const float u = c < 0.5f ? c * 2.0f : 2.0f - c * 2.0f;
    const float px = L.at(1.9f - u * span), pz = L.z + 1.0f * s;
    const float step = std::fabs(std::sin(t * 7.0f)) * 0.06f * s * motion();
    strata_kit::host().add(STRATA_PRIMITIVE_CAPSULE, px, 0.75f * s + step, pz, 0.32f * s, 0.32f * s, 0.32f * s,
                           {0.12f, 0.14f, 0.25f});
    ball(px, 1.22f * s + step, pz, 0.26f * s, 0.26f * s, 0.26f * s, {0.85f, 0.68f, 0.55f});
    box(px, 1.38f * s + step, pz, 0.34f * s, 0.08f * s, 0.34f * s, {0.06f, 0.06f, 0.1f});
    const float lx = px + (c < 0.5f ? -0.25f : 0.25f) * L.dir * s, ly = 0.85f * s + step, lz = pz - 0.1f * s;
    ball(lx, ly, lz, 0.16f * s, 0.2f * s, 0.16f * s, {1.0f, 0.85f, 0.35f});
    glow(lx, ly, lz, 12.0f, 0.9f * s, {1.0f, 0.8f, 0.35f}, 0.85f);
    light(lx, ly + 0.2f, lz, 3.5f, {1.0f, 0.75f, 0.35f}, 1.6f, 3);
}

// Web: two telegraph poles on the near side with sagging wires, a pulse
// running down each wire, and sparks at the front insulators.
inline void drawTelegraph(const Loco& L, float t) {
    const float s = L.s, px = L.at(1.0f), qx = L.at(-6.5f), pz = L.z + 1.25f * s, ph = 3.2f * s;
    const Rgb wood{0.35f, 0.24f, 0.14f}, spark{0.75f, 0.9f, 1.0f};
    for (float x : {px, qx}) {
        cyl(x, ph * 0.5f, pz, 0.07f * s, ph * 0.5f, wood);
        box(x, ph - 0.25f * s, pz, 0.12f * s, 0.1f * s, 1.0f * s, wood);
    }
    const auto sag = [s](float u) { return 4.0f * u * (1 - u) * 0.3f * s; };
    for (int w = -1; w <= 1; w += 2) {
        const float wz = pz + w * 0.4f * s, wy = ph - 0.15f * s;
        for (int k = 0; k < 8; ++k) {
            const float u0 = k / 8.0f, u1 = (k + 1) / 8.0f;
            line(px + (qx - px) * u0, wy - sag(u0), wz, px + (qx - px) * u1, wy - sag(u1), wz, kIron, 0.9f);
        }
        ball(px, wy, wz, 0.1f * s, 0.12f * s, 0.1f * s, {0.7f, 0.9f, 0.85f});
        const float u = fract(t * 0.9f + (w > 0 ? 0.5f : 0.0f));
        glow(px + (qx - px) * u, wy - sag(u), wz, 8.0f, 0.3f * s, {0.6f, 0.85f, 1.0f}, 0.95f);
    }
    const float tick = std::floor(t * 12.0f);
    for (int i = 0; i < 6; ++i) {
        const float k = tick + i * 7.0f;
        if (hash(k, L.seed) >= 0.55f) continue;
        glow(px + (hash(L.seed + k, 1.0f) - 0.5f) * 0.6f * s, ph + hash(L.seed + k, 3.0f) * 0.6f * s,
             pz + (hash(L.seed + k, 2.0f) - 0.5f) * 1.0f * s, 5.0f, 0.15f * s, spark, 1.0f);
    }
    light(px, ph + 0.3f, pz, 3.5f, {0.6f, 0.8f, 1.0f}, 1.2f + hash(tick, L.seed), 3);
}

// A column of steam from the whistle on the cab roof; `strength` scales it.
inline void whistleSteam(const Loco& L, float t, float strength, int puffs) {
    for (int i = 0; i < puffs; ++i) {
        const float age = fract(t * 1.5f + i / static_cast<float>(puffs));
        glow(L.at(0.9f), (1.9f + age * 1.4f * strength) * L.s, L.z, 4.0f, (0.15f + age * 0.5f * strength) * L.s, kWhite,
             0.7f * (1.0f - age));
    }
}

// Other tools: the whistle blows on and off.
inline void drawWhistle(const Loco& L, float t) {
    if (fract(t * 0.5f + L.seed) < 0.35f) whistleSteam(L, t, 1.0f, 6);
    ball(L.at(0.9f), 1.85f * L.s, L.z, 0.1f * L.s, 0.3f * L.s, 0.1f * L.s, kBrass);
}

// A new tool call: a short, tall blast from the whistle with a brass flash,
// fading over the kit's pulse (about a second).
inline void drawToolCall(const Loco& L, float t) {
    if (L.cold() || L.pulse < 0.02f) return;
    whistleSteam(L, t, 1.0f + 1.5f * L.pulse, 8);
    glow(L.at(0.9f), 2.0f * L.s, L.z, 12.0f, 0.5f * L.s * L.pulse, kBrass, L.pulse);
}

inline void drawActivity(const Loco& L, float t) {
    if (L.cold()) return;
    drawSmoke(L, t);
    drawToolCall(L, t);
    if (L.state != State::Busy || L.stalled) return;
    switch (L.action) {
        case Action::Think: drawCylinderSteam(L, t); break;
        case Action::Read: drawInspector(L, t); break;
        case Action::Web: drawTelegraph(L, t); break;
        case Action::Other: drawWhistle(L, t); break;
        default: break;  // edit (the crane) and run (wheels, smoke) are part of the train
    }
}

}  // namespace railway
