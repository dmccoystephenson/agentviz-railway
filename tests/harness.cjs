// Shared bits for the headless browser tests: serve the fixture on port 8943
// (this lane's 8843 + 100), launch Chromium (SwiftShader) and open the page.
//   NODE_PATH=/root/arcade-watch/node_modules node tests/smoke.cjs
const path = require("path");
const {spawn} = require("child_process");
const http = require("http");

const ROOT = path.resolve(__dirname, "..");
const PORT = Number(process.env.RAILWAY_TEST_PORT || 8943);
const BASE = `http://127.0.0.1:${PORT}`;

function loadPlaywright() {
  const tries = [undefined, "/root/arcade-watch/node_modules", "/root/casework/node_modules"];
  for (const p of tries) {
    try { return require(p ? path.join(p, "playwright") : "playwright"); } catch (e) { /* next */ }
  }
  throw new Error("playwright not found: set NODE_PATH to a node_modules that has it");
}

function get(url) {
  return new Promise((res, rej) => http.get(url, (r) => { r.resume(); res(r.statusCode); }).on("error", rej));
}

async function startServer() {
  const proc = spawn("python3", ["-m", "vendor.agentfeed", "serve", "--web", "web", "--port", String(PORT), "--fixture"],
    {cwd: ROOT, stdio: ["ignore", "pipe", "inherit"]});
  for (let i = 0; i < 100; i++) {
    try { if (await get(BASE + "/api/agents") === 200) break; } catch (e) { /* not up yet */ }
    await new Promise((r) => setTimeout(r, 100));
  }
  return {proc, stop: () => proc.kill()};
}

async function launch() {
  const {chromium} = loadPlaywright();
  return chromium.launch({args: ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"]});
}

// Opens /?fixture=1 (plus extra query) and waits for the kit to draw trains.
async function open(browser, {query = "&drive=1", viewport = {width: 1600, height: 900}, dpr = 1, mobile = false} = {}) {
  const ctx = await browser.newContext({viewport, deviceScaleFactor: dpr, isMobile: mobile, hasTouch: mobile});
  const page = await ctx.newPage();
  const errors = [];
  page.on("console", (m) => { if (m.type() === "error") errors.push(m.text()); });
  page.on("pageerror", (e) => errors.push(String(e)));
  await page.goto(`${BASE}/?fixture=1${query}`, {waitUntil: "commit"});
  await page.waitForFunction(() => window.__strataKit && window.__strataKit.ready && window.__railway
    && window.__railway.yard.frameTrains.length > 0, null, {timeout: 120000});
  return {page, ctx, errors};
}

// Under ?drive=1 the loop is off: draw scene times t0..t1 synchronously.
async function drive(page, t0, t1 = t0, step = 0.5) {
  for (let t = t0; t <= t1 + 1e-6; t += step) await page.evaluate((tt) => window.__strataKit.drive(tt), t);
}

// page.screenshot waits for a stable page, which a slow WebGL page may never
// be; the DevTools capture reads what is on screen now.
async function shot(page, file) {
  const fs = require("fs");
  const cdp = await page.context().newCDPSession(page);
  const {data} = await cdp.send("Page.captureScreenshot", {format: "png"});
  const buf = Buffer.from(data, "base64");
  if (file) fs.writeFileSync(file, buf);
  await cdp.detach();
  return buf;
}

module.exports = {ROOT, BASE, PORT, startServer, launch, open, drive, shot};
