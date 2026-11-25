// Regenerates every README image in docs/media/ from the FIXTURE feed:
//   NODE_PATH=/root/arcade-watch/node_modules node tests/screenshots.cjs [name ...]
// Frames are drawn with ?drive=1 (scene time stepped by hand), so they repeat.
// The GIF is joined with Pillow (tests/make_gif.py).
const fs = require("fs");
const os = require("os");
const path = require("path");
const {execFileSync} = require("child_process");
const {ROOT, startServer, launch, open, drive, shot} = require("./harness.cjs");

const OUT = path.join(ROOT, "docs/media");
const only = process.argv.slice(2);
const want = (n) => !only.length || only.includes(n);

// Points the camera at a train (by id) or at the whole yard.
async function aim(page, {id, yaw = 1.9, pitch = 0.62, dist = 22, dx = 0, dz = 0, select = null}) {
  await page.evaluate(({id, yaw, pitch, dist, dx, dz, select}) => {
    const R = window.__railway, cam = R.kit.cam;
    if (id) {
      const t = R.yard.frameTrains.find((tr) => tr.id === id);
      Object.assign(cam, {tx: t.x + dx, ty: 0.8, tz: t.z + dz, yaw, pitch, dist});
    }
    if (select !== null) R.select(select, false);
  }, {id, yaw, pitch, dist, dx, dz, select});
}

async function still(page, name, t = 6) {
  await drive(page, t - 1, t, 0.5);
  for (let i = 1; i <= 3; i++) {  // labels settle (placeLabels hysteresis)
    await page.waitForTimeout(450);
    await drive(page, t + i * 0.02);
  }
  await shot(page, path.join(OUT, name));
  console.log("wrote", name);
}

(async () => {
  fs.mkdirSync(OUT, {recursive: true});
  const server = await startServer();
  const browser = await launch();
  try {
    const desk = await open(browser, {viewport: {width: 1600, height: 900}});
    const page = desk.page;
    await drive(page, 0.5, 4);
    await page.waitForTimeout(1500);  // the loader fades out
    if (want("hero")) {
      await page.evaluate(() => window.__railway.fit());
      await still(page, "hero.png");
    }
    if (want("angle-high")) {
      await page.evaluate(() => { window.__railway.fit(); Object.assign(window.__railway.kit.cam, {yaw: 1.55, pitch: 1.2}); });
      await still(page, "angle-high.png");
    }
    if (want("angle-low")) {
      await page.evaluate(() => { window.__railway.fit(); const c = window.__railway.kit.cam;
        Object.assign(c, {yaw: 2.6, pitch: 0.38, dist: c.dist * 0.6}); });
      await still(page, "angle-low.png");
    }
    if (want("lead")) {
      await aim(page, {id: "demo-lead-b", yaw: 1.75, pitch: 0.7, dist: 34, dx: 12, dz: 4});
      await still(page, "lead-and-sidings.png");
    }
    if (want("waiting")) {
      await aim(page, {id: "demo-waiting", yaw: 1.9, pitch: 0.45, dist: 20, dz: 0});
      await still(page, "waiting.png");
    }
    if (want("idle-offline")) {
      await aim(page, {id: "demo-idle", yaw: 1.95, pitch: 0.6, dist: 24, dx: -8});
      await still(page, "idle-and-offline.png");
    }
    if (want("stalled")) {
      await aim(page, {id: "demo-thinker", yaw: 1.85, pitch: 0.5, dist: 26, dx: -7});
      await still(page, "stalled-and-editing.png");
    }
    if (want("resting")) {
      await aim(page, {id: "demo-a5", yaw: 2.1, pitch: 0.6, dist: 16, dx: -2, dz: -2});
      await still(page, "resting-subagent.png");
    }
    if (want("panel")) {
      await aim(page, {id: "demo-lead-b", yaw: 1.8, pitch: 0.6, dist: 30, dx: 8, dz: 3, select: "demo-lead-b"});
      await still(page, "panel-desktop.png");
      await aim(page, {select: ""});
    }
    if (desk.errors.length) console.error("console errors:\n  " + desk.errors.join("\n  "));
    await desk.ctx.close();
    if (want("gif")) {
      // Its own small context: SwiftShader's cost is per pixel.
      const g = await open(browser, {viewport: {width: 800, height: 450}});
      const page = g.page;
      await drive(page, 0.5, 4);
      await page.waitForTimeout(1500);
      await aim(page, {id: "demo-lead-a", yaw: 1.8, pitch: 0.68, dist: 19, dx: 9, dz: 4});
      // Send one subagent home mid-clip so the GIF shows a locomotive rolling.
      const dir = fs.mkdtempSync(path.join(os.tmpdir(), "railway-gif-"));
      await drive(page, 10, 11, 0.5);
      await page.waitForTimeout(400);
      await page.evaluate(() => {
        const doc = JSON.parse(JSON.stringify(window.__railway.yard.doc));
        const wren = doc.sessions.find((s) => s.id === "demo-lead-a");
        const f = wren.followers.find((x) => x.id === "demo-a1");
        Object.assign(f, {state: "resting", action: "none"});
        window.__railway.feed(doc);
      });
      const fps = 8, secs = 5;
      for (let i = 0; i < fps * secs; i++) {
        await page.evaluate((t) => window.__strataKit.drive(t), 11 + i / fps);
        await shot(page, path.join(dir, `f${String(i).padStart(3, "0")}.png`));
      }
      const gif = path.join(OUT, "motion.gif");
      execFileSync("python3", [path.join(__dirname, "make_gif.py"), dir, gif, String(fps), "800"]);
      fs.rmSync(dir, {recursive: true, force: true});
      console.log("wrote motion.gif", Math.round(fs.statSync(gif).size / 1024), "KB");
      await g.ctx.close();
    }
    if (want("phone")) {
      const ph = await open(browser, {viewport: {width: 390, height: 844}, dpr: 3, mobile: true});
      await drive(ph.page, 0.5, 4);
      await ph.page.waitForTimeout(1500);
      await aim(ph.page, {id: "demo-waiting", yaw: 1.9, pitch: 0.6, dist: 26, dx: 4, dz: 3, select: "demo-waiting"});
      await still(ph.page, "phone-sheet.png");
      await ph.ctx.close();
    }
  } finally {
    await browser.close();
    server.stop();
  }
})();
