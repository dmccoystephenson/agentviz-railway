// track.h: the ground, the trunk lines (one per place) and each session's
// yard (a ladder of sidings), from the frame's scene floats and zones.
#pragma once

#include "../vendor/agentviz-kit/scene/agentviz.h"
#include "props.h"

namespace railway {

// Zone kinds and extras (web/railway-frame.js).
constexpr int ZONE_TRUNK = 0, ZONE_YARD = 1;
constexpr int TRUNK_SHED_END = 0, TRUNK_TOOL_COUNT = 1;
constexpr int YARD_SIDINGS = 0, YARD_DELEGATING = 1;
constexpr int LINK_DELEGATION = 0;
constexpr float kGap = 1.7f;  // the ladder's step (web/railway-model.js GAP)

// Grass under the whole yard; scene floats 0..3 are the yard's extent.
inline void drawGround(const float* scene) {
    const float x0 = scene[0] - 6.0f, z0 = scene[1] - 2.0f, x1 = scene[2] + 8.0f, z1 = scene[3] + 4.0f;
    box((x0 + x1) * 0.5f, -0.1f, (z0 + z1) * 0.5f, x1 - x0, 0.2f, z1 - z0, {0.30f, 0.36f, 0.20f});
}

// Straight track along x: ballast, sleepers and two rails as boxes.
inline void railAlongX(float x0, float x1, float z, float ballastW) {
    const float len = x1 - x0, cx = (x0 + x1) * 0.5f;
    box(cx, 0.04f, z, len + 0.4f, 0.12f, ballastW, kBallast);
    for (float s = 0.3f; s < len; s += 0.62f) box(x0 + s, 0.12f, z, 0.22f, 0.07f, 0.95f, kSleeper);
    for (int side = -1; side <= 1; side += 2) box(cx, 0.2f, z + side * 0.36f, len, 0.09f, 0.07f, kSteel);
}

// Track at an angle (the yard ladder): the same in wide lines.
inline void railAngled(float x0, float z0, float x1, float z1, float ballastW) {
    const float dx = x1 - x0, dz = z1 - z0, len = std::sqrt(dx * dx + dz * dz);
    const float ux = dx / len, uz = dz / len, nx = -uz, nz = ux;  // along, across
    const float gauge = 0.36f;
    wline(x0, 0.06f, z0, x1, 0.06f, z1, kBallast, 4.0f, ballastW);
    for (float s = 0.3f; s < len; s += 0.62f) {
        const float cx = x0 + ux * s, cz = z0 + uz * s;
        wline(cx - nx * 0.48f, 0.13f, cz - nz * 0.48f, cx + nx * 0.48f, 0.13f, cz + nz * 0.48f, kSleeper, 2.0f, 0.2f);
    }
    for (int side = -1; side <= 1; side += 2)
        wline(x0 + nx * gauge * side, 0.21f, z0 + nz * gauge * side, x1 + nx * gauge * side, 0.21f,
              z1 + nz * gauge * side, kSteel, 1.5f, 0.08f);
}

// A trunk line: buffer stop, coal stage, station and engine shed at its
// head (x0), track to the yard's far end (x1).
inline void drawTrunk(const agentviz::Zone& zone) {
    const float x0 = zone.x0, z = zone.z0;
    railAlongX(x0, zone.x1, z, 1.5f);
    drawBuffer(x0 - 0.2f, z, 1.2f);
    drawCoalStage(x0 - 0.5f, z, zone.extra[TRUNK_TOOL_COUNT]);
    drawStation(x0 + 3.0f, z, zone.seed);
    drawShed(x0 + 5.5f, zone.extra[TRUNK_SHED_END], z, 3.2f);
}

// A session's yard: the ladder from its root switch (x0, z0) and one siding
// per subagent off it to a buffer stop at x1, each with a ground lever.
inline void drawYard(const agentviz::Zone& zone, float t) {
    const int sidings = agentviz::toInt(zone.extra[YARD_SIDINGS]);
    if (sidings <= 0) return;
    const bool delegating = zone.extra[YARD_DELEGATING] > 0.5f;
    const float rootX = zone.x0, rootZ = zone.z0, endX = zone.x1;
    railAngled(rootX, rootZ, rootX + kGap * sidings, rootZ + kGap * sidings, 1.25f);
    drawSwitch(rootX, rootZ - 0.95f, delegating, 0.0f, t);
    for (int k = 0; k < sidings; ++k) {
        const float bx = rootX + kGap * (k + 1), bz = rootZ + kGap * (k + 1);
        railAlongX(bx, endX, bz, 1.25f);
        drawBuffer(endX + 0.3f, bz, 0.8f);
        drawSwitch(bx + 0.9f, bz - 0.85f, delegating, k * 0.17f, t);
    }
}

// Two pulses running from the root switch up the ladder to a siding's branch
// point, then 6 m along the siding: the lead handing out work.
inline void drawDelegation(const agentviz::Link& link, float t) {
    const float ladder = std::hypot(link.x1 - link.x0, link.z1 - link.z0), along = 6.0f, total = ladder + along;
    for (int j = 0; j < 2; ++j) {
        const float u = fract(t * 0.45f + link.phase + j * 0.5f), d = u * total;
        float x = link.x1 + (d - ladder), z = link.z1;
        if (d < ladder) {
            const float k = d / std::max(ladder, 1e-3f);
            x = link.x0 + (link.x1 - link.x0) * k;
            z = link.z0 + (link.z1 - link.z0) * k;
        }
        glow(x, 0.45f, z, 10.0f, 0.6f, kSignalGreen, std::sin(u * kPi));
    }
}

inline void drawZones(const std::vector<agentviz::Zone>& zones, float t) {
    for (const auto& zone : zones) {
        if (zone.kind == ZONE_TRUNK) drawTrunk(zone);
        else if (zone.kind == ZONE_YARD) drawYard(zone, t);
    }
}

inline void drawLinks(const std::vector<agentviz::Link>& links, float t) {
    for (const auto& link : links)
        if (link.kind == LINK_DELEGATION) drawDelegation(link, t);
}

}  // namespace railway
