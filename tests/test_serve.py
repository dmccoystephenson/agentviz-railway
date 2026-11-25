"""Serves the fixture feed next to web/ and checks the page, its assets, the
feed and the vendored copies. Stdlib only: python3 -m unittest discover -s tests"""
import json
import os
import re
import socket
import subprocess
import sys
import time
import unittest
import urllib.error
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = int(os.environ.get("RAILWAY_TEST_PORT", "9104"))
BASE = "http://127.0.0.1:%d" % PORT

PAGE_SCRIPTS = ("strata-loader.js", "strata-kit.js", "agentviz-feed.js", "agentviz-screen.js", "agentviz-hud.js",
                "agentviz-wire.js", "railway-model.js", "railway-frame.js", "page.js")


def get(path):
    with urllib.request.urlopen(BASE + path, timeout=10) as r:
        return r.status, r.headers.get("Content-Type", ""), r.read()


class ServeFixtureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.proc = subprocess.Popen(
            [sys.executable, "-m", "vendor.agentfeed", "serve", "--web", "web", "--port", str(PORT), "--fixture"],
            cwd=REPO, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.time() + 15
        while time.time() < deadline:
            try:
                socket.create_connection(("127.0.0.1", PORT), timeout=0.5).close()
                return
            except OSError:
                time.sleep(0.1)
        cls.proc.kill()
        raise RuntimeError("server did not start")

    @classmethod
    def tearDownClass(cls):
        cls.proc.terminate()
        cls.proc.wait(timeout=10)

    def test_page_loads_the_kit_in_order(self):
        status, ctype, body = get("/")
        self.assertEqual(status, 200)
        self.assertIn("text/html", ctype)
        html = body.decode()
        positions = [html.index('src="%s"' % script) for script in PAGE_SCRIPTS]
        self.assertEqual(positions, sorted(positions), "scripts out of order")
        self.assertIn("agentviz-hud.css", html)

    def test_assets(self):
        for asset in PAGE_SCRIPTS + ("agentviz-hud.css", "railway-engine.js"):
            with self.subTest(asset=asset):
                status, _, body = get("/" + asset)
                self.assertEqual(status, 200)
                self.assertGreater(len(body), 500)
        status, ctype, body = get("/railway-engine.wasm")
        self.assertEqual(status, 200)
        self.assertEqual(ctype, "application/wasm")
        self.assertEqual(body[:4], b"\0asm")

    def test_engine_exports_only_the_wire_setter(self):
        _, _, body = get("/page.js")
        self.assertIn("createRailway", body.decode())
        engine = get("/railway-engine.js")[2].decode()
        self.assertIn("_agentviz_frame", engine)
        self.assertNotIn("_rw_set_", engine)

    def test_page_never_parses_feed_strings_as_html_or_swallows_errors(self):
        for script in ("railway-model.js", "railway-frame.js", "page.js"):
            source = get("/" + script)[2].decode()
            self.assertNotIn("innerHTML", source, script)
            self.assertIsNone(re.search(r"catch\s*(\([^)]*\))?\s*\{\s*\}", source), script + ": empty catch")

    def test_feed_parses(self):
        status, _, body = get("/api/agents")
        self.assertEqual(status, 200)
        doc = json.loads(body)
        self.assertEqual(doc["version"], 1)
        self.assertTrue(doc["fixture"])
        names = {s["name"]: s for s in doc["sessions"]}
        self.assertEqual(names["Thistle"]["state"], "waiting")
        self.assertEqual({s["waitingReason"] for s in doc["sessions"] if s["state"] == "waiting"},
                         {"question", "permission"})
        for s in doc["sessions"]:
            self.assertIn(s["state"], ("offline", "dim", "idle", "busy", "waiting"))
            self.assertIn(s["action"], ("edit", "run", "read", "web", "delegate", "think", "other", "none"))
            for f in s["followers"]:
                self.assertIn(f["state"], ("active", "resting"))
        self.assertTrue(any(f.get("stalled") for s in doc["sessions"] for f in s["followers"]))
        self.assertTrue(all("toolCount" in p for p in doc["places"]))

    def test_no_escape_from_web(self):
        with self.assertRaises(urllib.error.HTTPError) as e:
            get("/../vendor/agentfeed/feed.py")
        self.assertEqual(e.exception.code, 404)


class VendoredCopiesTest(unittest.TestCase):
    def test_vendored_copies_match_their_pins(self):
        result = subprocess.run([sys.executable, os.path.join(REPO, "vendor", "agentviz-kit", "check_vendor.py"), REPO],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
