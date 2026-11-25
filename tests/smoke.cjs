// Headless smoke test: serve the fixture, load /?fixture=1, wait for the kit to
// draw, check the frame is not flat, that every session and subagent is drawn,
// that a tap on a locomotive selects it and opens the panel, that the waiting
// banner shows, and that nothing logs an error. Saves docs/media/screenshot.png.
//   NODE_PATH=/root/arcade-watch/node_modules node tests/smoke.cjs
const path = require("path");
const {ROOT, startServer, launch, open, drive, shot} = require("./harness.cjs");

(async () => {
  const server = await startServer();
  const browser = await launch();
  let failed = false;
  const fail = (m) => { console.error("FAIL:", m); failed = true; };
  try {
    const {page, errors} = await open(browser);
    await drive(page, 0.5, 4);
    const st = await page.evaluate(() => window.__strataKit.stats());
    if (st.primitives < 200) fail(`too few primitives drawn: ${st.primitives}`);
    const trains = await page.evaluate(() => window.__railway.yard.frameTrains.map((t) => t.kind));
    const sessions = trains.filter((k) => k === "session").length, followers = trains.filter((k) => k === "follower").length;
    if (sessions !== 8) fail(`expected 8 locomotives for sessions, got ${sessions}`);
    if (followers !== 11) fail(`expected 11 subagent locomotives, got ${followers}`);
    const needs = await page.evaluate(() => !document.getElementById("needs").hidden
      && document.getElementById("needs").textContent);
    if (!needs || !needs.includes("Thistle")) fail(`waiting banner missing: ${needs}`);
    await shot(page, path.join(ROOT, "docs/media/screenshot.png"));
    const lit = await page.evaluate(() => {
      const c = document.getElementById("view"), g = c.getContext("webgl2");
      const w = g.drawingBufferWidth, h = g.drawingBufferHeight, px = new Uint8Array(w * h * 4);
      g.readPixels(0, 0, w, h, g.RGBA, g.UNSIGNED_BYTE, px);
      const seen = new Set();
      for (let i = 0; i < px.length; i += 4 * 97) seen.add((px[i] >> 4) << 8 | (px[i + 1] >> 4) << 4 | (px[i + 2] >> 4));
      return seen.size;
    });
    if (lit < 20) fail(`frame looks flat: ${lit} colours`);
    // Tap a locomotive: the panel opens with its name.
    const target = await page.evaluate(() => window.__strataKit.targets().find((t) => t.id === "demo-lead-b"));
    if (!target) fail("no tap target for Otto");
    else {
      await page.mouse.click(target.x, target.y);
      await drive(page, 4.5);
      const sheet = await page.evaluate(() => !document.getElementById("sheet").hidden
        && document.getElementById("sheet").textContent);
      if (!sheet || !sheet.includes("Otto")) fail(`panel did not open for Otto: ${sheet}`);
    }
    if (errors.length) fail("console errors:\n  " + errors.join("\n  "));
  } catch (e) {
    fail(e.stack || String(e));
  } finally {
    await browser.close();
    server.stop();
  }
  console.log(failed ? "smoke: FAIL" : "smoke: PASS");
  process.exit(failed ? 1 : 0);
})();
