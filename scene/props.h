// props.h: the fixed furniture of the yard: engine sheds, stations, coal
// stages, ground levers and buffer stops.
#pragma once

#include "draw_util.h"

namespace railway {

// An engine shed over the head of the trunk, cut away on the near (+z) side
// so the cold engines parked inside stay visible: back wall, end wall, the
// far roof slab with smoke vents, and pillars with a beam in front.
inline void drawShed(float x0, float x1, float z, float depth) {
    const Rgb brick{0.42f, 0.20f, 0.14f}, roof{0.20f, 0.21f, 0.24f}, pillar = scale(brick, 0.85f);
    const float w = x1 - x0, cx = (x0 + x1) * 0.5f, h = 2.9f, half = depth * 0.5f;
    box(cx, h * 0.5f, z - half, w, h, 0.25f, brick);
    box(x0, h * 0.5f, z, 0.25f, h, depth, brick);
    box(cx, h + 0.15f, z - half * 0.55f, w + 0.4f, 0.3f, half * 0.9f + 0.3f, roof);
    const float pitch = std::max(2.0f, w / std::max(1.0f, std::round(w / 4.5f)));
    for (float px = x0 + 0.2f; px <= x1 + 0.01f; px += pitch) box(px, h * 0.5f, z + half, 0.22f, h, 0.22f, pillar);
    box(cx, h - 0.1f, z + half, w, 0.25f, 0.25f, pillar);
    for (float vx = x0 + 1.5f; vx < x1 - 0.5f; vx += 3.0f) box(vx, h + 0.6f, z - half * 0.5f, 0.5f, 0.5f, 0.5f, kIron);
}

// A small signal box at the head of each trunk: the place's station, with
// a nameboard on posts in front (the page puts the place's name over it).
inline void drawStation(float x, float z, float seed) {
    const Rgb wall = mix({0.75f, 0.68f, 0.52f}, {0.62f, 0.55f, 0.45f}, seed);
    box(x, 1.0f, z - 2.2f, 2.2f, 2.0f, 1.6f, wall);
    box(x, 2.15f, z - 2.2f, 2.6f, 0.3f, 2.0f, {0.35f, 0.12f, 0.08f});
    box(x, 1.4f, z - 1.39f, 1.6f, 0.6f, 0.05f, {0.12f, 0.16f, 0.22f});
    box(x - 0.9f, 0.6f, z - 1.2f, 0.1f, 1.2f, 0.1f, kIron);
    box(x + 0.9f, 0.6f, z - 1.2f, 0.1f, 1.2f, 0.1f, kIron);
    box(x, 1.15f, z - 1.2f, 2.0f, 0.4f, 0.08f, {0.92f, 0.92f, 0.88f});
}

// A coal stage beside the station: a timber bunker whose heap grows with the
// tool calls seen across the place's sessions and subagents.
inline void drawCoalStage(float x, float z, float toolCount) {
    const Rgb timber{0.38f, 0.26f, 0.15f}, coal{0.05f, 0.05f, 0.06f};
    const float heap = std::min(1.4f, 0.2f + 0.4f * std::log10(1.0f + std::max(0.0f, toolCount)));
    box(x, 0.3f, z - 2.4f, 1.9f, 0.6f, 0.12f, timber);
    box(x - 0.9f, 0.3f, z - 2.9f, 0.12f, 0.6f, 1.0f, timber);
    box(x + 0.9f, 0.3f, z - 2.9f, 0.12f, 0.6f, 1.0f, timber);
    ball(x, 0.15f + heap * 0.35f, z - 2.9f, 1.7f, heap * 0.9f, 0.95f, coal);
    ball(x + 0.3f, 0.15f + heap * 0.2f, z - 2.75f, 0.9f, heap * 0.55f, 0.6f, scale(coal, 1.6f));
}

// A buffer stop at the end of a line.
inline void drawBuffer(float x, float z, float s) {
    box(x, 0.45f * s, z, 0.25f * s, 0.6f * s, 1.0f * s, {0.45f, 0.08f, 0.06f});
    for (int side = -1; side <= 1; side += 2)
        ball(x - 0.15f * s, 0.55f * s, z + side * 0.3f * s, 0.18f * s, 0.18f * s, 0.18f * s, kIron);
    box(x + 0.3f * s, 0.25f * s, z, 0.5f * s, 0.25f * s, 0.9f * s, kSleeper);
}

// How far an active lever has thrown (0 rest, 1 thrown): over, hold, back.
inline float leverThrow(float t, float phase) {
    if (motion() == 0) return 1.0f;
    const float c = fract(t * 0.55f + phase);
    return c < 0.5f ? std::min(1.0f, c * 6.0f) : std::max(0.0f, 1.0f - (c - 0.5f) * 6.0f);
}

// A ground lever beside a junction. Active levers (the lead is delegating)
// throw back and forth and their lamp flips white to green; idle ones rest.
inline void drawSwitch(float x, float z, bool active, float phase, float t) {
    box(x, 0.2f, z, 0.45f, 0.3f, 0.3f, scale(kIron, 2.0f));
    const float thrown = active ? leverThrow(t, phase) : 0.0f;
    const float angle = -0.6f + 1.2f * thrown;
    const float lx = x + std::sin(angle) * 0.75f, ly = 0.35f + std::cos(angle) * 0.75f;
    wline(x, 0.35f, z, lx, ly, z, {0.75f, 0.12f, 0.08f}, 3.0f, 0.08f);
    ball(lx, ly, z, 0.14f, 0.14f, 0.14f, {0.9f, 0.9f, 0.9f});
    const Rgb lamp = thrown > 0.5f ? kSignalGreen : Rgb{0.95f, 0.95f, 0.85f};
    ball(x, 0.75f, z - 0.05f, 0.18f, 0.18f, 0.18f, lamp);
    if (active) glow(x, 0.75f, z, 6.0f, 0.6f, lamp, 0.55f);
}

}  // namespace railway
