/* railway-frame.js: packs the yard for the scene, once a frame, in the
   agentviz wire format (vendor/agentviz-kit, docs/KIT.md in agentviz).
   scene/main.cpp and its headers read what is written here.

     scene floats  the yard's extent: x0, z0, x1, z1 (the ground pads it)
     zones         kind 0 trunk line (a place): x0..x1 along z0;
                     extras: shed end x, the place's tool count
                   kind 1 a session's yard: ladder root (x0, z0), siding end x1, last siding z1;
                     extras: sidings, delegating (0/1)
     agents        extras: scale (1 lead, 0.6 subagent), speed (0..1, wheels and smoke drift);
                   facing 0 looks down the line (+x), pi looks back at the trunk
     links         kind 0 delegation pulse: from the ladder root (x0, z0) to a siding's
                   branch point (x1, z1); phase staggers the sidings

   Classic script: window.RailwayFrame. Needs Railway (railway-model.js). */
(function () {
  "use strict";
  const {Yard, GAP, TRUNK_X0} = window.Railway;
  const ZONE = {trunk: 0, yard: 1};
  const LINK = {delegation: 0};

  const delegating = (s) => s.session.state === "busy" && s.session.action === "delegate";

  function trunks(wire, yard) {
    for (const p of yard.places.values()) {
      wire.zone({id: "place:" + p.id, kind: ZONE.trunk, seed: p.seed, x0: TRUNK_X0, z0: p.z, x1: yard.xEnd, z1: p.z,
        extra: [p.shedEnd, p.toolCount]});
    }
  }

  function yards(wire, yard) {
    for (const s of yard.sessions.values()) {
      if (!s.sidings) continue;
      const {rootX, rootZ, endX} = Yard.ladder(s);
      wire.zone({id: "yard:" + s.id, kind: ZONE.yard, x0: rootX, z0: rootZ, x1: endX, z1: rootZ + GAP * s.sidings,
        extra: [s.sidings, delegating(s) ? 1 : 0]});
    }
  }

  function locomotives(wire, yard, highlightOf, pulseOf) {
    for (const tr of yard.trains) {
      wire.agent({session: tr.session, follower: tr.follower, x: tr.x, y: 0, z: tr.z, facing: tr.dir > 0 ? 0 : Math.PI,
        highlight: highlightOf(tr.id), pulse: pulseOf(tr.id), extra: [tr.scale, tr.speed]});
    }
  }

  // Pulses run from a delegating lead's root switch up the ladder into each siding.
  function delegations(wire, yard) {
    for (const s of yard.sessions.values()) {
      if (!s.sidings || !delegating(s)) continue;
      const {rootX, rootZ} = Yard.ladder(s);
      for (let k = 0; k < s.sidings; k++) {
        const at = Yard.branch(s, k);
        wire.link({kind: LINK.delegation, from: s.id, x0: rootX, z0: rootZ, x1: at.x, z1: at.z, phase: k * 0.11});
      }
    }
  }

  // highlightOf(id) -> 0 none, 1 selected, 2 hovered; pulseOf(id) -> 0..1 after a new tool call.
  function write(wire, yard, highlightOf, pulseOf) {
    const b = yard.bounds();
    wire.begin({doc: yard.doc, scene: [b.x0, b.z0, b.x1, b.z1]});
    trunks(wire, yard);
    yards(wire, yard);
    locomotives(wire, yard, highlightOf, pulseOf);
    delegations(wire, yard);
    return wire;
  }

  window.RailwayFrame = {write, ZONE, LINK};
})();
