/* agentviz-railway page: polls /api/agents, lays the yard out, eases between
   snapshots and hands the Strata scene three float arrays per frame.

   Places are trunk lines (one per repo), sessions are locomotives on them,
   subagents are smaller locomotives on sidings off a yard ladder ahead of
   their lead. Offline sessions park in the engine shed at the head of the
   trunk. See README.md for the legend. */
"use strict";

const STATE_CODE = {offline: 0, dim: 1, idle: 2, busy: 3, waiting: 4, resting: 5};
const ACTION_CODE = {none: 0, edit: 1, run: 2, read: 3, web: 4, delegate: 5, think: 6, other: 7};
const ACTION_INFO = {
  edit: {icon: "\u{1F4E6}", text: "loading freight (editing)"},
  run: {icon: "\u{1F682}", text: "steaming (running a command)"},
  read: {icon: "\u{1F3EE}", text: "inspecting (reading)"},
  web: {icon: "\u{1F4E1}", text: "telegraph (web)"},
  delegate: {icon: "\u{1F500}", text: "throwing switches (delegating)"},
  think: {icon: "☁️", text: "idle steam (thinking)"},
  other: {icon: "⚙️", text: "working (other tool)"},
  none: {icon: "—", text: "no action"},
};
const STATE_INFO = {
  busy: {cls: "busy", text: "busy"}, idle: {cls: "idle", text: "idle"},
  waiting: {cls: "waiting", text: "waiting on you"}, offline: {cls: "offline", text: "offline (in the shed)"},
  dim: {cls: "dim", text: "dim"}, active: {cls: "busy", text: "active"}, resting: {cls: "idle", text: "resting (headed back to merge)"},
};

const GAP = 1.7;          // between sidings
const FOLLOWER_SCALE = 0.6;
const SIDING_LEN = 16;
const BAY = 9.5;          // one engine shed bay
const PLAIN_SLOT = 14;    // a session with no sidings
const RAIL = 5, TRAIN = 10, PROP = 6;
const PR = {shed: 0, sw: 1, buffer: 2, glow: 3, station: 4, ground: 5};

const params = new URLSearchParams(location.search);
const FIXTURE = params.get("fixture") === "1";

/* ------------------------------------------------------------------ layout */

// Target layout for one feed document: per place a trunk z; per session a
// slot x; per follower its siding index and where on the siding it sits.
function layout(doc) {
  const byPlace = new Map();
  for (const s of doc.sessions || []) {
    if (!byPlace.has(s.place)) byPlace.set(s.place, []);
    byPlace.get(s.place).push(s);
  }
  const placeIds = [...byPlace.keys()].sort();
  const placeMeta = new Map((doc.places || []).map((p) => [p.id, p]));
  const places = [], sessions = new Map();
  let z = 0, xEnd = 20;
  for (const pid of placeIds) {
    const list = byPlace.get(pid);
    const off = list.filter((s) => s.state === "offline");
    const on = list.filter((s) => s.state !== "offline");
    const shedEnd = Math.max(1, off.length) * BAY + 0.5;
    off.forEach((s, i) => sessions.set(s.id, {sx: i * BAY, z, cx: i * BAY + 6.3, n: 0, inShed: true}));
    let x = shedEnd + 2, maxN = 0;
    for (const s of on) {
      const n = Math.min((s.followers || []).length, 8);
      maxN = Math.max(maxN, n);
      const cx = x + 6.5;
      const width = n ? (cx + 3.2 - x) + GAP * n + SIDING_LEN + 3 : PLAIN_SLOT;
      sessions.set(s.id, {sx: x, z, cx, n, inShed: false});
      x += width;
    }
    xEnd = Math.max(xEnd, x + 4);
    const meta = placeMeta.get(pid) || {};
    places.push({id: pid, label: meta.label || pid, seed: meta.seed || 0, z, shedEnd});
    z += GAP * maxN + 8;
  }
  return {places, sessions, xEnd, zEnd: z - 8};
}

/* ------------------------------------------------------------------ easing */

const ease = (cur, target, k) => cur + (target - cur) * k;

class Yard {
  constructor() {
    this.doc = null;
    this.places = new Map();    // id -> {z, target, label, seed, shedEnd}
    this.sessions = new Map();  // id -> {data, cx, z, tx, tz, followers: Map}
    this.xEnd = 20; this.xEndT = 20; this.zEnd = 0;
    this.bufs = {rails: [0, 0], trains: [0, 0], props: [0, 0]};
    this.frameTrains = [];
  }

  update(doc) {
    this.doc = doc;
    const L = layout(doc);
    const seen = new Set();
    for (const p of L.places) {
      seen.add(p.id);
      const cur = this.places.get(p.id);
      if (cur) Object.assign(cur, {target: p.z, label: p.label, seed: p.seed, shedEndT: p.shedEnd});
      else this.places.set(p.id, {id: p.id, z: p.z, target: p.z, label: p.label, seed: p.seed, shedEnd: p.shedEnd, shedEndT: p.shedEnd});
    }
    for (const id of [...this.places.keys()]) if (!seen.has(id)) this.places.delete(id);
    const seenS = new Set();
    for (const s of doc.sessions || []) {
      seenS.add(s.id);
      const t = L.sessions.get(s.id);
      let cur = this.sessions.get(s.id);
      if (!cur) {
        cur = {id: s.id, cx: t.cx, z: t.z, followers: new Map()};
        this.sessions.set(s.id, cur);
      }
      Object.assign(cur, {data: s, tcx: t.cx, tz: t.z, n: t.n, inShed: t.inShed});
      const fs = (s.followers || []).slice(0, 8);
      const seenF = new Set();
      fs.forEach((f, k) => {
        seenF.add(f.id);
        const pos = f.state === "resting" ? SIDING_LEN - 4.2 : 5.0;
        let fc = cur.followers.get(f.id);
        if (!fc) { fc = {id: f.id, k, pos}; cur.followers.set(f.id, fc); }
        Object.assign(fc, {data: f, lead: cur, tk: k, tpos: pos, tdir: f.state === "resting" ? -1 : 1});
        if (fc.dir == null) fc.dir = fc.tdir;
      });
      for (const id of [...cur.followers.keys()]) if (!seenF.has(id)) cur.followers.delete(id);
    }
    for (const id of [...this.sessions.keys()]) if (!seenS.has(id)) this.sessions.delete(id);
    this.xEndT = L.xEnd;
    this.zEnd = L.zEnd;
  }

  step(dt) {
    const k = 1 - Math.exp(-dt * 2.5);
    for (const p of this.places.values()) { p.z = ease(p.z, p.target, k); p.shedEnd = ease(p.shedEnd, p.shedEndT, k); }
    this.xEnd = ease(this.xEnd, this.xEndT, k);
    for (const s of this.sessions.values()) {
      const px = s.cx;
      s.cx = ease(s.cx, s.tcx, k); s.z = ease(s.z, s.tz, k);
      s.speed = Math.min(1, Math.abs(s.cx - px) / Math.max(dt, 1e-3) / 4);
      for (const f of s.followers.values()) {
        const pp = f.pos;
        f.k = ease(f.k, f.tk, k);
        f.pos = ease(f.pos, f.tpos, k * 0.6);
        f.speed = Math.min(1, Math.abs(f.pos - pp) / Math.max(dt, 1e-3) / 3);
        // Turn round once it has nearly arrived (a resting engine faces the trunk).
        if (Math.abs(f.pos - f.tpos) < 0.5) f.dir = f.tdir;
      }
    }
  }

  // World positions of every locomotive this frame (for drawing, labels, picking).
  trains() {
    const out = [];
    for (const s of this.sessions.values()) {
      out.push({kind: "session", id: s.id, data: s.data, x: s.cx, z: s.z, s: 1, dir: 1, speed: s.speed || 0, lead: null});
      const rx = s.cx + 3.2;
      for (const f of s.followers.values()) {
        const bx = rx + GAP * (f.k + 1), bz = s.z + GAP * (f.k + 1);
        out.push({kind: "follower", id: f.id, data: f.data, x: bx + f.pos, z: bz, s: FOLLOWER_SCALE, dir: f.dir,
          speed: f.speed || 0, lead: s.data, sidingEnd: rx + GAP * s.n + SIDING_LEN, k: f.k});
      }
    }
    return out;
  }

  write(mod, t, sel) {
    const rails = [], props = [], trains = [];
    const x0 = -9, x1 = this.xEnd;
    let zMax = 0;
    props.push(PR.ground, x0 - 6, -8, x1 + 8, 0, 0);  // z1 patched below
    for (const p of this.places.values()) {
      rails.push(-6, p.z, x1, p.z, 0);
      props.push(PR.buffer, -6.2, p.z, 1.2, 0, 0);
      props.push(PR.station, -3, p.z, p.seed, 0, 0);
      props.push(PR.shed, -0.5, p.z, p.shedEnd, 3.2, 0);
      zMax = Math.max(zMax, p.z);
    }
    for (const s of this.sessions.values()) {
      if (!s.n) continue;
      const rx = s.cx + 3.2, n = s.n, zEnd = s.cx + 3.2 + GAP * n + SIDING_LEN;
      const delegating = s.data.state === "busy" && s.data.action === "delegate";
      rails.push(rx, s.z, rx + GAP * n, s.z + GAP * n, 1);
      props.push(PR.sw, rx, s.z - 0.95, delegating ? 1 : 0, 0, 0);
      for (let k = 0; k < n; k++) {
        const bx = rx + GAP * (k + 1), bz = s.z + GAP * (k + 1);
        rails.push(bx, bz, zEnd, bz, 1);
        props.push(PR.buffer, zEnd + 0.3, bz, 0.8, 0, 0);
        props.push(PR.sw, bx + 0.9, bz - 0.85, delegating ? 1 : 0, k * 0.17, 0);
        zMax = Math.max(zMax, bz);
        if (delegating) {
          // Pulses run from the root switch up the ladder into each siding.
          for (let j = 0; j < 2; j++) {
            const u = (t * 0.45 + k * 0.11 + j * 0.5) % 1;
            const ladder = (k + 1) * GAP * Math.SQRT2, along = 6, total = ladder + along;
            const d = u * total;
            let gx, gz;
            if (d < ladder) { gx = rx + d / Math.SQRT2; gz = s.z + d / Math.SQRT2; } else { gx = bx + (d - ladder); gz = bz; }
            props.push(PR.glow, gx, gz, 1, Math.sin(u * Math.PI), 0);
          }
        }
      }
    }
    props[4] = zMax + 8;
    this.frameTrains = this.trains();
    for (const tr of this.frameTrains) {
      const d = tr.data || {};
      let state, action;
      if (tr.kind === "session") {
        state = STATE_CODE[d.state] != null ? STATE_CODE[d.state] : 1;
        action = ACTION_CODE[d.action] || 0;
      } else {
        state = d.state === "resting" ? STATE_CODE.resting : STATE_CODE.busy;
        action = d.state === "resting" ? 0 : (ACTION_CODE[d.action] || 7);
      }
      let flags = 0;
      if (sel && sel.id === tr.id) flags |= 1;
      if (d.stalled) flags |= 2;
      if (hovered && hovered.id === tr.id) flags |= 4;
      trains.push(tr.x, tr.z, tr.s, tr.dir, state, action, d.seed != null ? d.seed : 0.5, flags, tr.speed,
        tr.kind === "session" ? 1 : 0);
    }
    this._send(mod, "rails", rails, RAIL, "_rw_set_rails");
    this._send(mod, "props", props, PROP, "_rw_set_props");
    this._send(mod, "trains", trains, TRAIN, "_rw_set_trains");
    this.bounds = {x0, x1, z0: -6, z1: zMax + 4};
  }

  _send(mod, key, arr, stride, fn) {
    const b = this.bufs[key];
    if (arr.length > b[1]) {
      if (b[0]) mod._free(b[0]);
      b[1] = arr.length + 256;
      b[0] = mod._malloc(b[1] * 4);
    }
    if (arr.length) mod.HEAPF32.set(arr, b[0] >> 2);
    mod[fn](b[0], arr.length / stride);
  }
}

/* ------------------------------------------------------------------ page */

const yard = new Yard();
let held = false, kit = null, mod = null, selected = null, hovered = null, lastT = 0, fitted = false;
const labelsEl = document.getElementById("labels");
const labelEls = new Map();  // key -> element

function labelFor(key, cls, text) {
  let el = labelEls.get(key);
  if (!el) {
    el = document.createElement("div");
    labelsEl.appendChild(el);
    labelEls.set(key, el);
  }
  if (el.className !== "label " + cls) el.className = "label " + cls;
  if (el.textContent !== text) el.textContent = text;
  el._used = true;
  return el;
}

function placeAllLabels() {
  for (const el of labelEls.values()) el._used = false;
  const items = [];
  for (const p of yard.places.values()) {
    items.push({el: labelFor("p:" + p.id, "place", p.label), q: kit.project(-3, 3.0, p.z - 2.2), rank: 0});
  }
  for (const tr of yard.frameTrains) {
    const d = tr.data || {};
    if (tr.kind === "session") {
      const waiting = d.state === "waiting";
      const cls = "name" + (waiting ? " waiting" : "") + (d.state === "offline" ? " offline" : "");
      items.push({el: labelFor("s:" + tr.id, cls, (waiting ? "\u{1F514} " : "") + (d.name || "session")),
        q: kit.project(tr.x - 0.5, waiting ? 3.6 : 3.1, tr.z), rank: waiting || (selected && selected.id === tr.id) ? 0 : 1});
      if (d.branch && d.state !== "offline") {
        items.push({el: labelFor("b:" + tr.id, "branch", d.branch), q: kit.project(tr.x - 2.5, 0.9, tr.z + 0.65), rank: 2});
      }
    } else {
      items.push({el: labelFor("f:" + tr.id, "lane" + (d.state === "resting" ? " resting" : ""), d.label || "subagent"),
        q: kit.project(tr.sidingEnd + 0.6, 0.9, tr.z), rank: selected && selected.id === tr.id ? 0 : 2});
    }
  }
  for (const [key, el] of labelEls) if (!el._used) { el.remove(); labelEls.delete(key); }
  StrataKit.placeLabels(items);
}

function pick(x, y, radius) {
  let best = null, bestD = radius * radius * 2.5;
  for (const tr of yard.frameTrains) {
    for (const off of [0, -1.2, 1.2, -2.6]) {
      const q = kit.project(tr.x + off * tr.s * tr.dir, 1.2 * tr.s, tr.z);
      if (!q) continue;
      const d = (q.x - x) ** 2 + (q.y - y) ** 2;
      if (d < bestD) { bestD = d; best = tr; }
    }
  }
  return best;
}

/* ------------------------------------------------------------------ panel */

const sheet = document.getElementById("sheet");
function esc(s) { return String(s == null ? "" : s).replace(/[&<>"]/g, (c) => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;"}[c])); }

function renderPanel() {
  if (!selected) { sheet.hidden = true; return; }
  const tr = yard.frameTrains.find((t) => t.id === selected.id);
  if (!tr) { selected = null; sheet.hidden = true; return; }
  const d = tr.data;
  let html;
  if (tr.kind === "session") {
    const st = STATE_INFO[d.state] || {cls: "dim", text: d.state};
    const act = ACTION_INFO[d.action] || ACTION_INFO.other;
    const label = d.activity && d.activity.label;
    const fl = (d.followers || []).map((f) => {
      const a = ACTION_INFO[f.state === "resting" ? "none" : f.action] || ACTION_INFO.other;
      return `<li><button data-id="${esc(f.id)}"><span class="sig ${f.state === "resting" ? "idle" : "busy"}"></span>` +
        `<span class="ic">${a.icon}</span>${esc(f.label || "subagent")}</button></li>`;
    }).join("");
    html = `<header><span class="sig ${st.cls}"></span><h2>${esc(d.name || "session")}</h2>` +
      `<button class="close" aria-label="Close">×</button></header>` +
      `<dl><dt>State</dt><dd>${esc(st.text)}${d.stalled ? " · <b class=stall>stalled</b>" : ""}</dd>` +
      `<dt>Action</dt><dd><span class="ic">${act.icon}</span>${esc(act.text)}</dd>` +
      (label ? `<dt>Label</dt><dd>${esc(label)}</dd>` : "") +
      `<dt>Branch</dt><dd><code>${esc(d.branch || "—")}</code></dd>` +
      `<dt>Line</dt><dd>${esc(d.placeLabel || d.place)}</dd></dl>` +
      (fl ? `<h3>Sidings (${(d.followers || []).length})</h3><ul class="fl">${fl}</ul>` : "");
  } else {
    const st = STATE_INFO[d.state] || STATE_INFO.active;
    const act = ACTION_INFO[d.state === "resting" ? "none" : d.action] || ACTION_INFO.other;
    html = `<header><span class="sig ${st.cls}"></span><h2>${esc(d.label || "subagent")}</h2>` +
      `<button class="close" aria-label="Close">×</button></header>` +
      `<dl><dt>State</dt><dd>${esc(st.text)}</dd>` +
      `<dt>Action</dt><dd><span class="ic">${act.icon}</span>${esc(act.text)}</dd>` +
      (d.tool ? `<dt>Tool</dt><dd>${esc(d.tool)}${d.toolLabel ? " · " + esc(d.toolLabel) : ""}</dd>` : "") +
      `<dt>Lead</dt><dd><button class="lead" data-id="${esc(tr.lead.id)}">${esc(tr.lead.name || "session")}</button></dd>` +
      `<dt>Branch</dt><dd><code>${esc(tr.lead.branch || "—")}</code></dd></dl>`;
  }
  if (sheet._html !== html) { sheet.innerHTML = html; sheet._html = html; }
  sheet.hidden = false;
}

sheet.addEventListener("click", (e) => {
  const b = e.target.closest("button");
  if (!b) return;
  if (b.classList.contains("close")) return select(null);
  if (b.dataset.id) select(yard.frameTrains.find((t) => t.id === b.dataset.id) || null, true);
});

function select(tr, fly) {
  selected = tr ? {id: tr.id} : null;
  if (kit) kit.selected = tr;
  if (tr && fly) flyTo(tr.x, tr.z);
  renderPanel();
}

let flight = null;
function flyTo(x, z) {
  const c = kit.cam;
  flight = {from: {tx: c.tx, tz: c.tz, dist: c.dist}, to: {tx: x, tz: z, dist: Math.min(c.dist, 26)}, t0: performance.now()};
}

/* ------------------------------------------------------------------ waiting banner */

const banner = document.getElementById("needs");
function renderBanner() {
  const waiting = (yard.doc && yard.doc.sessions || []).filter((s) => s.state === "waiting");
  const html = waiting.map((s) => `<button data-id="${esc(s.id)}"><span class="bell">\u{1F514}</span>${esc(s.name || "session")}</button>`).join("");
  if (banner._html !== html) { banner.innerHTML = html; banner._html = html; }
  banner.hidden = !waiting.length;
}
banner.addEventListener("click", (e) => {
  const b = e.target.closest("button");
  if (b) select(yard.frameTrains.find((t) => t.id === b.dataset.id) || null, true);
});

/* ------------------------------------------------------------------ feed */

const statusEl = document.getElementById("status");
async function poll() {
  try {
    const r = await fetch("/api/agents" + (FIXTURE ? "?fixture=1" : ""), {cache: "no-store"});
    if (!r.ok) throw new Error("HTTP " + r.status);
    const doc = await r.json();
    if (held) return;
    yard.update(doc);
    statusEl.textContent = doc.fixture ? "demo feed" : "";
    statusEl.hidden = !doc.fixture;
    renderBanner();
    if (!fitted && kit) fit();
  } catch (e) {
    statusEl.textContent = "feed unreachable";
    statusEl.hidden = false;
  }
}

function fit() {
  if (!yard.places.size) return;
  fitted = true;
  const L = layout(yard.doc);
  const cx = (-9 + L.xEnd) / 2, cz = L.zEnd / 2 + 1;
  const span = Math.max(L.xEnd + 9, (L.zEnd + 10) * 1.6);
  const aspect = innerWidth / Math.max(1, innerHeight);
  Object.assign(kit.cam, {tx: cx, ty: 0, tz: cz, dist: span * (aspect < 1 ? 1.1 / aspect : 0.72), yaw: 1.9, pitch: 0.72});
  if (kit.limits) kit.limits.maxDist = Math.max(kit.limits.maxDist || 0, kit.cam.dist * 1.5);
}

/* ------------------------------------------------------------------ boot */

(async () => {
  try {
    mod = await StrataLoader.load({script: "railway-engine.js", factory: "createRailway", wasm: "railway-engine.wasm",
      title: "the railway yard"});
  } catch (e) { return; }
  const canvas = document.getElementById("view");
  try {
    kit = StrataKit.create({
      module: mod, canvas,
      camera: {yaw: 1.9, pitch: 0.72, dist: 90, tx: 30, ty: 0, tz: 15},
      limits: {maxDist: 260, minDist: 5},
      pick,
      idOf: (tr) => tr.id,
      onTap(tr) { select(tr || null, false); },
      onHover(tr) { hovered = tr; canvas.style.cursor = tr ? "pointer" : ""; },
      onSlow: () => StrataLoader.slow(),
      onFatal: (code, msg) => StrataLoader.fallback(code, msg),
      frame(t) {
        const dt = Math.min(kit.driven ? 1 : 0.1, Math.max(0, t - lastT));
        lastT = t;
        if (flight) {
          const u = Math.min(1, (performance.now() - flight.t0) / 700), e = u * u * (3 - 2 * u);
          for (const k of ["tx", "tz", "dist"]) kit.cam[k] = flight.from[k] + (flight.to[k] - flight.from[k]) * e;
          if (u >= 1) flight = null;
        }
        yard.step(dt);
        yard.write(mod, t, selected);
      },
      after() { placeAllLabels(); renderPanel(); },
      targets: () => {
        const r = canvas.getBoundingClientRect();
        return yard.frameTrains.map((tr) => {
          const q = kit.project(tr.x, 1.2 * tr.s, tr.z);
          return q && {id: tr.id, x: q.x + r.left, y: q.y + r.top};
        }).filter(Boolean);
      },
    });
  } catch (e) {
    return StrataLoader.fallback(e.code === "no-webgl2" ? "no-webgl2" : "engine");
  }
  kit.setSun(Date.UTC(2026, 5, 21, 23, 0, 0) / 1000, 40, -105);
  await poll();
  fit();
  kit.start();
  if (kit.driven) kit.drive(0);
  StrataLoader.done();
  setInterval(poll, 2000);
  window.__railway = {yard, select: (id) => select(yard.frameTrains.find((t) => t.id === id) || null, true),
    get kit() { return kit; }, fit,
    // For capture tools: show this document and stop taking polls.
    feed(doc) { held = true; yard.update(doc); renderBanner(); }};
})();
