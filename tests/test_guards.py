"""The kit's guard tests (vendor/agentviz-kit/tests/guards.py): no innerHTML in
the page's own scripts, no empty catch blocks, no machine-local paths."""
import importlib.util
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_spec = importlib.util.spec_from_file_location(
    "guards", os.path.join(REPO, "vendor", "agentviz-kit", "tests", "guards.py"))
guards = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(guards)

# Every emsdk build sets HOME to this path in its virtual filesystem; it is
# not a path on any real machine, so it is ignored in the built engine.
EMSCRIPTEN_HOME = "/" + "home/web_user"


class Guards(guards.GuardTests):
    REPO = REPO

    def test_no_machine_paths_in_tracked_text(self):
        for name, path in guards.tracked_text_files(self.REPO):
            if os.path.basename(name) in guards.ALLOWED_PATH_FILES:
                continue
            with open(path, encoding="utf-8", errors="replace") as f:
                text = f.read()
            if "-engine." in name:
                text = text.replace(EMSCRIPTEN_HOME, "")
            for marker in guards.MACHINE_PATHS:
                self.assertNotIn(marker, text, "%s contains %s" % (name, marker))
