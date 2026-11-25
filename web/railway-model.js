/* railway-model.js: lays the yard out from a feed document and eases every
   locomotive towards its place between snapshots.

   Places are trunk lines (one per repo, sorted by id), sessions are
   locomotives on them, subagents are smaller locomotives on sidings off a
   yard ladder just ahead of their lead. Offline sessions park in the engine
   shed at the head of the trunk. Everything is keyed by id: sessions by
   theirs, subagents by "<session id>/<follower id>" (AgentvizFeed.followerKey).

   Classic script: window.Railway. Needs AgentvizFeed. */
(function () {
  "use strict";
  const {ease, followerKey} = window.AgentvizFeed;

  const GAP = 1.7;            // between sidings, and the ladder's step
  const FOLLOWER_SCALE = 0.6;
  const SIDING_LEN = 16;
  const MAX_SIDINGS = 8;      // the feed sends at most 8 followers
  const BAY = 9.5;            // one engine shed bay
  const PLAIN_SLOT = 14;      // a session with no sidings
  const LADDER_AHEAD = 3.2;   // the ladder's root switch, ahead of the lead's centre
  const TRUNK_GAP = 8;        // between trunk lines
  const TRUNK_X0 = -6, YARD_X0 = -9, YARD_Z0 = -6;
  const RATE = 2.5;           // easing, 1/seconds
  const ACTIVE_POS = 5.0, RESTING_BACK = 4.2;

  /* ------------------------------------------------------------ layout */

  function groupByPlace(doc) {
    const byPlace = new Map();
    for (const s of doc.sessions || []) {
      if (!byPlace.has(s.place)) byPlace.set(s.place, []);
      byPlace.get(s.place).push(s);
    }
    return byPlace;
  }

  // One trunk: offline sessions in shed bays, then each session's slot,
  // wide enough for its ladder and sidings. Returns where the trunk ends.
  function layTrunk(list, z, sessions) {
    const offline = list.filter((s) => s.state === "offline");
    const shedEnd = Math.max(1, offline.length) * BAY + 0.5;
    offline.forEach((s, i) => sessions.set(s.id, {cx: i * BAY + 6.3, z, sidings: 0}));
    let x = shedEnd + 2, most = 0;
    for (const s of list.filter((one) => one.state !== "offline")) {
      const sidings = Math.min((s.followers || []).length, MAX_SIDINGS);
      most = Math.max(most, sidings);
      const cx = x + 6.5;
      sessions.set(s.id, {cx, z, sidings});
      x += sidings ? (cx + LADDER_AHEAD - x) + GAP * sidings + SIDING_LEN + 3 : PLAIN_SLOT;
    }
    return {shedEnd, xEnd: x + 4, depth: GAP * most + TRUNK_GAP};
  }

  // Target layout for one feed document: per place a trunk z and shed size;
  // per session a centre x on its trunk and how many sidings it has.
  function layout(doc) {
    const byPlace = groupByPlace(doc);
    const meta = new Map((doc.places || []).map((p) => [p.id, p]));
    const places = [], sessions = new Map();
    let z = 0, xEnd = 20;
    for (const id of [...byPlace.keys()].sort()) {
      const trunk = layTrunk(byPlace.get(id), z, sessions);
      const info = meta.get(id) || {};
      places.push({id, label: info.label || id, seed: info.seed || 0, toolCount: info.toolCount || 0, z,
        shedEnd: trunk.shedEnd});
      xEnd = Math.max(xEnd, trunk.xEnd);
      z += trunk.depth;
    }
    return {places, sessions, xEnd, zEnd: Math.max(0, z - TRUNK_GAP)};
  }

  /* ------------------------------------------------------------ the yard */

  class Yard {
    constructor() {
      this.doc = null;
      this.places = new Map();    // id -> {z, shedEnd and their targets, label, seed, toolCount}
      this.sessions = new Map();  // id -> {session, cx, z, targets, sidings, followers: Map}
      this.xEnd = 20; this.xEndTarget = 20; this.zEnd = 0;
      this.trains = [];           // this frame's locomotives (drawing, labels, picking)
    }

    update(doc) {
      this.doc = doc;
      const target = layout(doc);
      this._updatePlaces(target.places);
      this._updateSessions(doc, target.sessions);
      this.xEndTarget = target.xEnd;
      this.zEnd = target.zEnd;
    }

    _updatePlaces(places) {
      const seen = new Set();
      for (const p of places) {
        seen.add(p.id);
        const cur = this.places.get(p.id) || {z: p.z, shedEnd: p.shedEnd};
        this.places.set(p.id, Object.assign(cur, {id: p.id, label: p.label, seed: p.seed, toolCount: p.toolCount,
          zTarget: p.z, shedEndTarget: p.shedEnd}));
      }
      for (const id of [...this.places.keys()]) if (!seen.has(id)) this.places.delete(id);
    }

    _updateSessions(doc, slots) {
      const seen = new Set();
      for (const s of doc.sessions || []) {
        seen.add(s.id);
        const slot = slots.get(s.id);
        let cur = this.sessions.get(s.id);
        if (!cur) {
          cur = {id: s.id, cx: slot.cx, z: slot.z, speed: 0, followers: new Map()};
          this.sessions.set(s.id, cur);
        }
        Object.assign(cur, {session: s, cxTarget: slot.cx, zTarget: slot.z, sidings: slot.sidings});
        this._updateFollowers(cur, s);
      }
      for (const id of [...this.sessions.keys()]) if (!seen.has(id)) this.sessions.delete(id);
    }

    // A follower's siding is its index; an active one works near the ladder,
    // a resting one rolls to the far end and turns to face the trunk.
    _updateFollowers(lead, s) {
      const seen = new Set();
      (s.followers || []).slice(0, MAX_SIDINGS).forEach((f, siding) => {
        const key = followerKey(s, f);
        seen.add(key);
        const resting = f.state !== "active";
        const pos = resting ? SIDING_LEN - RESTING_BACK : ACTIVE_POS;
        let cur = lead.followers.get(key);
        if (!cur) { cur = {id: key, siding, pos, dir: resting ? -1 : 1, speed: 0}; lead.followers.set(key, cur); }
        Object.assign(cur, {follower: f, sidingTarget: siding, posTarget: pos, dirTarget: resting ? -1 : 1});
      });
      for (const key of [...lead.followers.keys()]) if (!seen.has(key)) lead.followers.delete(key);
    }

    step(dt) {
      for (const p of this.places.values()) {
        p.z = ease(p.z, p.zTarget, dt, RATE);
        p.shedEnd = ease(p.shedEnd, p.shedEndTarget, dt, RATE);
      }
      this.xEnd = ease(this.xEnd, this.xEndTarget, dt, RATE);
      const perSecond = (moved) => moved / Math.max(dt, 1e-3);
      for (const s of this.sessions.values()) {
        const before = s.cx;
        s.cx = ease(s.cx, s.cxTarget, dt, RATE);
        s.z = ease(s.z, s.zTarget, dt, RATE);
        s.speed = Math.min(1, perSecond(Math.abs(s.cx - before)) / 4);
        for (const f of s.followers.values()) {
          const was = f.pos;
          f.siding = ease(f.siding, f.sidingTarget, dt, RATE);
          f.pos = ease(f.pos, f.posTarget, dt, RATE * 0.6);
          f.speed = Math.min(1, perSecond(Math.abs(f.pos - was)) / 3);
          if (Math.abs(f.pos - f.posTarget) < 0.5) f.dir = f.dirTarget;  // turns round once nearly there
        }
      }
      this.trains = this._trains();
    }

    // Where the ladder of a session's yard starts and its sidings end.
    static ladder(s) {
      const rootX = s.cx + LADDER_AHEAD;
      return {rootX, rootZ: s.z, endX: rootX + GAP * s.sidings + SIDING_LEN};
    }
    // The branch point of siding k (fractional while easing) on a session's ladder.
    static branch(s, k) {
      const root = Yard.ladder(s);
      return {x: root.rootX + GAP * (k + 1), z: s.z + GAP * (k + 1)};
    }

    _trains() {
      const out = [];
      for (const s of this.sessions.values()) {
        out.push({id: s.id, session: s.session, follower: null, x: s.cx, z: s.z, scale: 1, dir: 1, speed: s.speed});
        const {endX} = Yard.ladder(s);
        for (const f of s.followers.values()) {
          const at = Yard.branch(s, f.siding);
          out.push({id: f.id, session: s.session, follower: f.follower, x: at.x + f.pos, z: at.z,
            scale: FOLLOWER_SCALE, dir: f.dir, speed: f.speed, sidingEnd: endX});
        }
      }
      return out;
    }

    // The yard's extent, eased (ground, camera fit).
    bounds() {
      let zMax = 0;
      for (const p of this.places.values()) zMax = Math.max(zMax, p.z);
      for (const s of this.sessions.values()) if (s.sidings) zMax = Math.max(zMax, s.z + GAP * s.sidings);
      return {x0: YARD_X0, x1: this.xEnd, z0: YARD_Z0, z1: zMax + 4};
    }
  }

  window.Railway = {Yard, layout, GAP, SIDING_LEN, TRUNK_X0};
})();
