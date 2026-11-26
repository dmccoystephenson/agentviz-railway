// Regenerates every README image in docs/media/ from the DEMO feed (never live):
//   npm run screenshots [-- name ...]
// Stills are drawn with ?drive=1 (the kit draws one frame on demand at a given
// scene time), so they repeat. The GIF steps scene time frame by frame while
// pushing two scripted changes (a subagent finishing, a new tool call) and is
// encoded with Pillow.
const fs = require("fs");
const os = require("os");
const path = require("path");
const h = require("../vendor/agentviz-kit/tests/harness.cjs");
const screens = require("../vendor/agentviz-kit/tests/screens.cjs");

const REPO = path.resolve(__dirname, "..");
const MEDIA = path.join(REPO, "docs/media");
const PORT = Number(process.env.RAILWAY_TEST_PORT || 9104);
const READY = "window.__page && window.__page.polls > 0 && window.__yard.trains.length > 0";
const only = new Set(process.argv.slice(2));
const want = (name) => only.size === 0 || only.has(name);
const DESKTOP = {viewport: {width: 1600, height: 900}};
const PHONE = {viewport: {width: 390, height: 844}, dpr: 3, mobile: true};

// Camera helpers run in the page: frame a train by id, or the whole yard.
const HELPERS = `
window.__shots = {
  t: 6,
  settle() { for (let t = 0.5; t <= 6; t += 0.5) window.__page.kit.drive(t); },
  cam(c) { Object.assign(window.__page.kit.cam, c); },
  train(id, c) { const tr = window.__yard.trains.find((x) => x.id === id);
    this.cam(Object.assign({tx: tr.x + (c.dx || 0), ty: 0.8, tz: tr.z + (c.dz || 0)}, c)); },
  draw() { window.__page.kit.drive(window.__shots.t); },
};`;

async function openPage(browser, server, options) {
  const opened = await h.open(browser, `${server.base}/?fixture=1&drive=1`, Object.assign({ready: READY}, options));
  await h.stillAnimations(opened.page);
  await opened.page.evaluate(() => window.__page.stopPolling());
  await opened.page.evaluate(HELPERS);
  await opened.page.evaluate(() => window.__shots.settle());
  return opened;
}

async function shoot(page, name, setup) {
  if (!want(name)) return;
  await page.evaluate(setup);
  await screens.still(page, path.join(MEDIA, name + ".png"), () => window.__shots.draw());
}

const DESKTOP_SHOTS = [
  ["hero", "window.__page.select(null); window.__page.fit(true)"],
  ["angle-high", "window.__page.fit(true); window.__shots.cam({yaw: 1.55, pitch: 1.2})"],
  ["angle-low", "window.__page.fit(true); window.__shots.cam({yaw: 2.6, pitch: 0.38, dist: window.__page.kit.cam.dist * 0.6})"],
  ["lead-and-sidings", "window.__shots.train('demo-lead-b', {yaw: 1.75, pitch: 0.7, dist: 36, dx: 13, dz: 5})"],
  ["waiting", "window.__shots.train('demo-waiting', {yaw: 1.95, pitch: 0.35, dist: 18, dx: 1, ty: 3})"],
  ["question", "window.__shots.train('demo-asker', {yaw: 1.95, pitch: 0.3, dist: 13, dx: 1.5, ty: 3.4})"],
  ["idle-and-offline", "window.__shots.train('demo-idle', {yaw: 1.95, pitch: 0.55, dist: 21, dx: -9, dz: -2.5})"],
  ["stalled", "window.__shots.train('demo-stalled', {yaw: 1.85, pitch: 0.45, dist: 15, dx: 0, ty: 1.8})"],
  ["resting-subagent", "window.__shots.train('demo-lead-a/demo-a5', {yaw: 2.1, pitch: 0.6, dist: 16, dx: -2, dz: -2})"],
  ["panel-desktop", "window.__shots.train('demo-lead-b', {yaw: 1.8, pitch: 0.6, dist: 32, dx: 9, dz: 4}); window.__page.select('demo-lead-b', false)"],
];

async function desktopStills(browser, server) {
  const {ctx, page, errors} = await openPage(browser, server, DESKTOP);
  for (const [name, setup] of DESKTOP_SHOTS) await shoot(page, name, setup);
  h.assertNoErrors(errors);
  await ctx.close();
}

async function phone(browser, server) {
  if (!want("phone-sheet")) return;
  const {ctx, page, errors} = await openPage(browser, server, PHONE);
  await shoot(page, "phone-sheet",
    "window.__shots.train('demo-waiting', {yaw: 1.9, pitch: 0.6, dist: 24, dx: -2, dz: 3}); window.__shots.draw(); " +
    "window.__page.select('demo-waiting', false)");
  h.assertNoErrors(errors);
  await ctx.close();
}

// The GIF: Wren's yard for five seconds at 8 frames a second. Early on one
// subagent finishes and rolls to the end of its siding; mid-clip another
// makes a tool call and blows its whistle.
async function motionGif(browser, server) {
  if (!want("motion")) return;
  const {ctx, page, errors} = await openPage(browser, server, {viewport: {width: 960, height: 540}});
  await page.evaluate(() => {
    const base = JSON.parse(JSON.stringify(window.__page.doc));
    const wren = base.sessions.find((s) => s.id === "demo-lead-a");
    window.__gif = {
      finish() { Object.assign(wren.followers.find((f) => f.id === "demo-a1"), {state: "resting", action: "none"});
        window.__page.feed(JSON.parse(JSON.stringify(base))); },
      toolCall() { wren.followers.find((f) => f.id === "demo-a2").toolCount += 1;
        window.__page.feed(JSON.parse(JSON.stringify(base))); },
    };
    for (let t = 6.5; t <= 11; t += 0.5) window.__page.kit.drive(t);
    window.__shots.train("demo-lead-a", {yaw: 1.8, pitch: 0.7, dist: 21, dx: 11, dz: 4});
  });
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "railway-gif-"));
  const FPS = 8;
  await screens.frames(page, tmp, FPS * 5, (i) => {
    const t = 11 + i / 8;
    if (i === 2) { window.__page.kit.drive(t); window.__gif.finish(); }
    if (i === 20) { window.__page.kit.drive(t); window.__gif.toolCall(); }
    window.__page.kit.drive(t);
  });
  screens.gif(tmp, path.join(MEDIA, "motion.gif"), FPS, 800);
  fs.rmSync(tmp, {recursive: true, force: true});
  h.assertNoErrors(errors);
  await ctx.close();
}

h.run("screenshots", {repo: REPO, port: PORT}, async ({server, browser}) => {
  fs.mkdirSync(MEDIA, {recursive: true});
  await desktopStills(browser, server);
  await phone(browser, server);
  await motionGif(browser, server);
});
