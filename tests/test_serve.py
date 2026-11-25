"""Serves the fixture with the vendored agentfeed and checks the page, its
assets and the feed. Run: python3 -m unittest discover -s tests"""
import json
import os
import socket
import subprocess
import sys
import time
import unittest
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = int(os.environ.get("RAILWAY_UNIT_PORT", "8943"))
BASE = "http://127.0.0.1:%d" % PORT
ASSETS = ["/", "/index.html", "/railway.js", "/railway-engine.js", "/railway-engine.wasm",
          "/strata-kit.js", "/strata-loader.js"]


def fetch(path):
    with urllib.request.urlopen(BASE + path, timeout=10) as r:
        return r.status, r.headers.get("Content-Type", ""), r.read()


class ServeFixture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.proc = subprocess.Popen(
            [sys.executable, "-m", "vendor.agentfeed", "serve", "--web", "web", "--port", str(PORT), "--fixture"],
            cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", PORT), timeout=0.2).close()
                break
            except OSError:
                time.sleep(0.1)

    @classmethod
    def tearDownClass(cls):
        cls.proc.terminate()
        cls.proc.wait(timeout=10)

    def test_page_and_assets(self):
        for path in ASSETS:
            with self.subTest(path=path):
                status, ctype, body = fetch(path)
                self.assertEqual(status, 200)
                self.assertGreater(len(body), 100)
        self.assertIn("application/wasm", fetch("/railway-engine.wasm")[1])
        page = fetch("/")[2].decode()
        for script in ("strata-loader.js", "strata-kit.js", "railway.js"):
            self.assertIn(script, page)

    def test_feed_parses(self):
        status, ctype, body = fetch("/api/agents")
        self.assertEqual(status, 200)
        doc = json.loads(body)
        self.assertEqual(doc["version"], 1)
        self.assertTrue(doc["fixture"])
        states = {s["state"] for s in doc["sessions"]}
        self.assertTrue({"busy", "idle", "waiting", "offline"} <= states)
        self.assertEqual(sum(len(s["followers"]) for s in doc["sessions"]), 11)
        for s in doc["sessions"]:
            self.assertIn("place", s)
            self.assertIn("action", s)

    def test_page_uses_feed_fields(self):
        js = fetch("/railway.js")[2].decode()
        for field in ("followers", "branch", "stalled", "waiting", "/api/agents", "fixture=1"):
            self.assertIn(field, js)

    def test_unknown_path_404(self):
        with self.assertRaises(urllib.error.HTTPError) as cm:
            fetch("/../vendor/agentfeed/feed.py")
        self.assertEqual(cm.exception.code, 404)


if __name__ == "__main__":
    unittest.main()
