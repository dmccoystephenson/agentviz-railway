# agentviz-railway

A railway-yard view of Claude Code sessions, one of the agentviz family
(https://github.com/dmccoystephenson/agentviz). Read README.md first.

- `vendor/agentfeed/` is a vendored copy: never edit it; re-vendor with
  `/root/agentviz/tools/vendor-feed.sh /root/agentviz-railway`.
- Scene: `scene/main.cpp` (C++, Strata web kit); rebuild with `scene/build.sh`
  and commit the outputs in `web/` (CI does not run emcc).
- Page: `web/index.html` + `web/railway.js` (layout, easing, labels, panel).
- Run: `python3 -m vendor.agentfeed serve --web web --port 8843 --fixture`.
- Tests: `python3 -m unittest discover -s tests` and
  `NODE_PATH=/root/arcade-watch/node_modules node tests/smoke.cjs`.
- Screenshots come from the fixture feed only, never from live sessions.
