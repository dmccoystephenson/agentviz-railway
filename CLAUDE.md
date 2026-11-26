# CLAUDE.md

agentviz-railway: Claude Code sessions drawn as a railway yard (Strata web kit). Part of the
agentviz family: https://github.com/dmccoystephenson/agentviz. Read README.md first.

- Data comes only from `/api/agents` (vendor/agentfeed; schema in the agentviz repo's docs/FEED.md).
- Shared pieces come from the agentviz kit (docs/KIT.md there): `vendor/agentviz-kit/` and the
  `web/agentviz-*` copies. Never edit `vendor/` or `web/agentviz-*`; re-vendor from an agentviz
  checkout with `tools/vendor-kit.sh <this repo>`. `tests/test_serve.py` checks the PINs.
- Page: `web/railway-model.js` lays out trunks, slots and sidings and eases the trains;
  `web/railway-frame.js` packs each frame in the agentviz wire format; `web/page.js` boots the
  feed client, HUD, labels, picking and camera; `web/index.html` holds the theme and the legend.
- Scene: `scene/main.cpp` decodes the wire (`agentviz.h`) and calls `track.h` (ground, trunks,
  yards, delegation pulses), `props.h`, `loco.h`, `signal.h` and `effects.h`. Build:
  `EMSDK=/path/to/emsdk vendor/agentviz-kit/scene/build-scene.sh scene/kit.conf` (emsdk 6.0.11,
  Strata 0.10.0). The built `web/railway-engine.*` are committed (CI does not run emcc).
- Run: `python3 -m vendor.agentfeed serve --web web --port 8843 [--fixture]`.
- Tests: `python3 -m unittest discover -s tests`; browser tests after `npm ci` and
  `npx playwright install chromium`: `npm run smoke`, `npm run screenshots` (Pillow from
  requirements-dev.txt for the GIF). Tests use port 9104 (`RAILWAY_TEST_PORT` overrides).
- Screenshots come from the FIXTURE feed only, never live sessions.
