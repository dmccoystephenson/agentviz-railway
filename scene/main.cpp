// agentviz-railway: Claude Code sessions as a railway yard, on the Strata web kit.
//
// The page (web/railway-*.js) owns the data and the layout: it reads the
// agent feed, lays out trunk lines and sidings and eases every locomotive
// between snapshots. Every frame it packs the scene in the agentviz wire
// format (vendor/agentviz-kit, docs/KIT.md in the agentviz repo) and calls
// agentviz_frame(); this file only draws what the frame says:
//   scene floats  the yard's extent, for the ground (track.h)
//   zones         trunk lines with their station, shed and coal stage, and
//                 each session's ladder of sidings (track.h, props.h)
//   agents        one locomotive per session or subagent (loco.h), its
//                 signal (signal.h) and what it is doing (effects.h)
//   links         delegation pulses up the ladder (track.h)
#define STRATA_KIT_MAIN
#include "strata_kit.h"

#include "effects.h"
#include "signal.h"
#include "track.h"

namespace {

agentviz::Frame frame;

}  // namespace

AGENTVIZ_FRAME_EXPORT(frame)

void strata_kit_configure(strata_kit::Options& o) {
    o.name = "agentviz-railway";
    o.farPlane = 600.0f;
}

void strata_kit_engine_ready(strata_kit::Host&) {}

void strata_kit_build(strata_kit::Host& h, double seconds) {
    h.ambientTint = {1.0f, 0.97f, 0.92f};
    if (!frame.ok) return;
    const float t = static_cast<float>(seconds);
    railway::drawGround(frame.scene);
    railway::drawZones(frame.zones, t);
    railway::drawLinks(frame.links, t);
    for (const auto& agent : frame.agents) {
        const railway::Loco loco = railway::locoOf(agent);
        railway::drawTrain(loco, t);
        railway::drawSignal(loco, t);
        railway::drawActivity(loco, t);
    }
    // A low warm ambient fill (the light itself is off) so the shaded sides of the yard keep their colour.
    StrataPointLight fill{};
    fill.position[1] = 40.0f;
    fill.range = 1.0f;
    fill.color[0] = fill.color[1] = fill.color[2] = 1.0f;
    h.fill(fill, {0.06f, 0.055f, 0.05f}, -1);
}
