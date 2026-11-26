/* page.js: boots the railway yard. The feed client, HUD, labels, picking and
   wire come from the agentviz kit (agentviz-*.js); this file wires them to
   the yard model (railway-model.js), packs frames (railway-frame.js) and owns
   the camera. */
(function () {
  "use strict";
  const {Yard, layout} = window.Railway;
  const REASON = {question: "has a question", permission: "needs permission"};
  const DEFAULT_YAW = 1.9, DEFAULT_PITCH = 0.72;
  // A portrait screen is narrow and the yard is about square: look down more
  // steeply and squarer on, so the yard fills the width instead of a strip.
  const PORTRAIT_YAW = 1.75, PORTRAIT_PITCH = 0.95, PORTRAIT_ZOOM = 0.8;
  const PHONE_WIDTH = 640;  // agentviz-hud.css: below this the card is a bottom sheet
  const canvas = document.getElementById("view");
  const yard = new Yard();
  const wire = AgentvizWire.create();
  const labels = AgentvizScreen.labels(document.getElementById("labels"));
  let kit = null, mod = null, feed = null, selectedId = null, hoveredId = null, camAnim = null, fitted = false;
  let sceneTime = 0, lastT = 0;

  /* ------------------------------------------------------------ camera */
  function fitView() {
    const target = layout(yard.doc);
    const x0 = -9, x1 = target.xEnd, depth = target.zEnd + 12;
    const span = Math.max(x1 - x0, depth * 1.9);
    const aspect = canvas.clientWidth / Math.max(1, canvas.clientHeight);
    const view = {tx: (x0 + x1) / 2, ty: 0, tz: target.zEnd * 0.55 + 2};
    if (aspect < 1) return Object.assign(view, {yaw: PORTRAIT_YAW, pitch: PORTRAIT_PITCH, dist: span * PORTRAIT_ZOOM / aspect});
    return Object.assign(view, {yaw: DEFAULT_YAW, pitch: DEFAULT_PITCH, dist: span * 0.62});
  }
  function fit(instant) {
    if (!kit || !yard.places.size) return;
    const view = fitView();
    kit.limits.maxDist = Math.max(kit.limits.maxDist || 0, view.dist * 1.5);
    flyTo(view, instant);
  }
  function flyTo(to, instant) {
    if (instant || kit.driven) { Object.assign(kit.cam, to); camAnim = null; return; }
    camAnim = {from: Object.assign({}, kit.cam), to, t0: performance.now(), ms: 700};
  }
  function stepCam() {
    if (!camAnim) return;
    const k = Math.min(1, (performance.now() - camAnim.t0) / camAnim.ms), e = AgentvizFeed.smoothstep(k);
    for (const key of Object.keys(camAnim.to)) kit.cam[key] = AgentvizFeed.lerp(camAnim.from[key], camAnim.to[key], e);
    if (k >= 1) camAnim = null;
  }
  function focusOn(tr) { flyTo({tx: tr.x, ty: 0, tz: tr.z, dist: Math.min(kit.cam.dist, 26)}); }
  // On a phone the card is a bottom sheet over the lower half: if it would
  // cover the train just tapped, slide the view (no zoom, no turn) so the
  // train sits in the clear upper part of the screen.
  function keepClearOfSheet(tr) {
    if (!kit || canvas.clientWidth > PHONE_WIDTH) return;
    const h = canvas.clientHeight, at = (dx, dz) => kit.project(tr.x + dx, 1.2 * tr.scale, tr.z + dz);
    const q = at(0, 0);
    if (!q || q.y < h * 0.42) return;
    const fx = -Math.cos(kit.cam.yaw), fz = -Math.sin(kit.cam.yaw);  // screen-up on the ground (strata-kit.js)
    const ahead = at(fx, fz), perUnit = ahead ? q.y - ahead.y : 0;
    if (perUnit <= 0.5) return;
    const u = (q.y - h * 0.3) / perUnit;
    flyTo({tx: kit.cam.tx - fx * u, tz: kit.cam.tz - fz * u});
  }

  /* ------------------------------------------------------------ labels */
  function sessionLabels(tr, items) {
    const s = tr.session, waiting = s.state === "waiting", offline = s.state === "offline";
    const text = waiting ? "\u{1F514} " + (s.name || s.id) + " " + (REASON[s.waitingReason] || "needs you") : s.name || s.id;
    const el = labels.set("s:" + tr.id, {cls: "name" + (waiting ? " waiting" : "") + (offline ? " offline" : ""),
      state: s.stalled ? "stalled" : s.state, action: s.state === "busy" ? s.action : null, text});
    items.push({el, q: kit.project(tr.x - 0.5, waiting ? 6.2 : 3.1, tr.z), rank: waiting || tr.id === selectedId ? 0 : 1});
    if (s.branch && !offline) {
      const plate = labels.set("b:" + tr.id, {cls: "branch", text: s.branch});
      items.push({el: plate, q: kit.project(tr.x - 2.5, 0.9, tr.z + 0.65), rank: 2});
    }
  }
  // The lane name sits at the siding's buffer stop; a subagent on its own
  // branch (a worktree lane) carries its own brass plate.
  function followerLabels(tr, items) {
    const f = tr.follower, resting = f.state !== "active";
    const el = labels.set("f:" + tr.id, {cls: "lane" + (resting ? " resting" : "") + (f.stalled ? " stalled" : ""),
      state: f.stalled ? "stalled" : null, action: resting ? "none" : f.action, text: f.label || f.id});
    items.push({el, q: kit.project(tr.sidingEnd + 0.6, 0.9, tr.z), rank: tr.id === selectedId ? 0 : 2});
    if (f.branch && f.branch !== tr.session.branch) {
      const plate = labels.set("fb:" + tr.id, {cls: "branch", text: f.branch});
      items.push({el: plate, q: kit.project(tr.x - 1.5 * tr.dir, 0.6, tr.z + 0.4), rank: 3});
    }
  }
  function placeLabels() {
    const items = [];
    labels.begin();
    for (const p of yard.places.values()) {
      items.push({el: labels.set("p:" + p.id, {cls: "place", text: p.label}), q: kit.project(-3, 3.0, p.z - 2.2), rank: 0});
    }
    for (const tr of yard.trains) (tr.follower ? followerLabels : sessionLabels)(tr, items);
    labels.end();
    placeClear(items);
  }

  // Keep world labels out from under the HUD: the top bar's chips, the open
  // card or sheet, the legend. Strata kit 1.4 takes these as placeLabels'
  // topInset / avoid; until the vendored kit is 1.4 the same rectangles go
  // in as placed-first stand-in labels (rank -1, sizes preset so nothing is
  // measured), and a never-hidden label that still reaches into the top band
  // is moved down to just under it, as 1.4 does.
  const KIT_AVOIDS = (() => {
    const [major, minor] = String(StrataKit.version).split(".").map(Number);
    return major > 1 || (major === 1 && minor >= 4);
  })();
  const HUD_PARTS = ".av-bar > :not([hidden]):not(.av-spacer), .av-panel:not([hidden]), .av-legend:not([hidden])";
  let hudRects = [], topInset = 0, hudReadAt = -1e9;
  function readHud() {
    const now = performance.now();
    if (now - hudReadAt < 250) return;
    hudReadAt = now;
    const pad = 4, c = canvas.getBoundingClientRect();
    hudRects = [...document.querySelectorAll(HUD_PARTS)].map((node) => node.getBoundingClientRect())
      .filter((r) => r.width > 0 && r.height > 0)
      .map((r) => ({left: r.left - c.left - pad, top: r.top - c.top - pad, right: r.right - c.left + pad,
        bottom: r.bottom - c.top + pad}));
    const bar = document.querySelector(".av-bar");
    // On a phone the bar spans the width: keep the whole band clear.
    topInset = bar && canvas.clientWidth <= PHONE_WIDTH ? bar.getBoundingClientRect().bottom - c.top + pad : 0;
  }
  const standIns = [];
  function standIn(i, r) {
    if (!standIns[i]) { standIns[i] = document.createElement("span"); standIns[i]._skText = ""; }
    const el = standIns[i];
    el._skW = r.right - r.left; el._skH = r.bottom - r.top;
    return {el, q: {x: (r.left + r.right) / 2, y: r.bottom, w: 0}, rank: -1, tries: 1};
  }
  function placeClear(items) {
    readHud();
    if (KIT_AVOIDS) { StrataKit.placeLabels(items, {topInset, avoid: hudRects}); keepOnScreen(items); return; }
    const rects = topInset > 0 ? [{left: -1e5, top: -1e5, right: 1e5, bottom: topInset}].concat(hudRects) : hudRects;
    StrataKit.placeLabels(rects.map((r, i) => standIn(i, r)).concat(items));
    keepOnScreen(items);
  }
  // A never-hidden label (a waiting train's, the selected one's) is pulled
  // back inside the screen's sides, so "needs you" is never cut off.
  function keepOnScreen(items) {
    const w = canvas.clientWidth, margin = 4;
    for (const it of items) {
      const el = it.el;
      if (it.rank > 0 || !el._skShown || !it.q) continue;
      const bottom = parseFloat(el.style.top);
      if (!KIT_AVOIDS && bottom - el._skH < topInset) el.style.top = (topInset + el._skH) + "px";
      const half = el._skW / 2, x = Math.min(Math.max(it.q.x, half + margin), w - half - margin);
      if (x !== it.q.x) el.style.left = x + "px";
    }
  }

  /* ------------------------------------------------------------ picking + selection */
  // Points along the engine, tender and wagon, so any part of the train taps.
  function pick(x, y, radius) {
    const candidates = yard.trains.map((tr) => ({item: tr,
      points: [0, -1.2, 1.2, -2.6].map((off) => [tr.x + off * tr.scale * tr.dir, 1.2 * tr.scale, tr.z])}));
    return AgentvizScreen.pick(kit.project, candidates, x, y, radius * 1.5);
  }
  const trainById = (id) => yard.trains.find((tr) => tr.id === id) || null;
  const targetOf = (tr) => (tr ? {session: tr.session, follower: tr.follower} : null);
  const idOf = (target) => (target ? (target.follower ? AgentvizFeed.followerKey(target.session, target.follower)
    : target.session.id) : null);

  function onSelect(target, how) {
    selectedId = idOf(target);
    const tr = selectedId && trainById(selectedId);
    if (kit) kit.selected = tr || null;
    if (tr && (how === "list" || how === "waiting")) focusOn(tr);
    else if (tr) keepClearOfSheet(tr);
  }

  const hud = AgentvizHud.create({
    title: "Railway Yard",
    legend: document.getElementById("legend"),
    buttons: [{icon: "home", label: "Recentre the view", onClick: () => fit(false)}],
    onSelect,
  });

  function accept(doc) {
    yard.update(doc);
    if (!yard.trains.length) yard.step(0);  // first snapshot: trains exist before the first frame
    hud.update(doc);
    if (!fitted && yard.places.size) { fitted = true; fit(true); }
  }

  /* ------------------------------------------------------------ boot */
  function frame(t) {
    const dt = Math.min(kit.driven ? 1 : 0.1, Math.max(0, t - lastT));
    lastT = sceneTime = t;
    stepCam();
    yard.step(dt);
    const highlight = (id) => (id === selectedId ? 1 : id === hoveredId ? 2 : 0);
    window.RailwayFrame.write(wire, yard, highlight, feed.pulse).send(mod);
  }

  function screenTargets() {
    const r = canvas.getBoundingClientRect();
    return yard.trains.map((tr) => {
      const q = kit.project(tr.x, 1.2 * tr.scale, tr.z);
      return q && {id: tr.id, x: q.x + r.left, y: q.y + r.top};
    }).filter(Boolean);
  }

  function createKit() {
    return StrataKit.create({
      module: mod, canvas,
      camera: {yaw: DEFAULT_YAW, pitch: DEFAULT_PITCH, dist: 90, tx: 30, ty: 0, tz: 15},
      limits: {maxDist: 260, minDist: 5},
      frame,
      after: placeLabels,
      pick,
      idOf: (tr) => tr.id,
      onTap: (tr) => hud.select(targetOf(tr), "tap"),
      onHover(tr) { hoveredId = tr ? tr.id : null; canvas.style.cursor = tr ? "pointer" : ""; },
      onSlow: () => StrataLoader.slow(),
      onFatal: (code, msg) => StrataLoader.fallback(code, msg),
      targets: screenTargets,
    });
  }

  async function boot() {
    try {
      mod = await StrataLoader.load({script: "railway-engine.js", factory: "createRailway", wasm: "railway-engine.wasm",
        title: "the railway yard"});
    } catch (error) {
      console.error("railway: the engine did not load", error);
      return;  // StrataLoader has already shown its fallback
    }
    try {
      kit = createKit();
    } catch (error) {
      StrataLoader.fallback(error.code === "no-webgl2" ? "no-webgl2" : "engine");
      return;
    }
    // Under ?drive=1 tool-call pulses follow scene time, so captures repeat.
    const now = () => (kit.driven ? sceneTime : Date.now() / 1000);
    feed = AgentvizFeed.create({onUpdate: accept, onStatus: hud.setStatus, now});
    window.__yard = yard;
    window.__page = {kit, yard, hud, fit, flyTo,
      select: (id, focus) => hud.select(targetOf(trainById(id)), focus ? "list" : "api"),
      get doc() { return feed.doc; }, get polls() { return feed.polls; },
      feedClient: feed, stopPolling: () => feed.stop(), pollOnce: () => feed.pollOnce(), feed: (d) => feed.push(d)};
    kit.setSun(Date.UTC(2026, 5, 21, 23, 0, 0) / 1000, 40, -105);  // a fixed late afternoon
    feed.start();
    kit.start();
    if (kit.driven) kit.drive(0);
    StrataLoader.done();
  }

  boot();
})();
