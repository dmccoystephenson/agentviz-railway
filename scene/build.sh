#!/usr/bin/env bash
# Builds scene/main.cpp into web/railway-engine.{js,wasm} with the Strata 0.10.0
# web kit. Downloads the release tarball once into the gitignored .strata/.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
STRATA_VERSION=0.10.0
STRATA_SHA256=bb86b974c7e0309db91625ba5ee157273704c77d93f4338c8a63ee625e3d56de
tarball=".strata/strata-$STRATA_VERSION-wasm32-emscripten.tar.gz"
mkdir -p .strata
if [ ! -f "$tarball" ]; then
  gh release download "v$STRATA_VERSION" -R Preponderous-Software/strata \
    -p "strata-$STRATA_VERSION-wasm32-emscripten.tar.gz" -D .strata
fi
echo "$STRATA_SHA256  $tarball" | sha256sum -c --quiet -
[ -f .strata/kit/build-kit.sh ] || tar -xzf "$tarball" -C .strata
# emcc 6.0.11 (emsdk_env.sh clears EMSDK_PYTHON; the helper handles that).
if ! command -v emcc >/dev/null 2>&1; then . /root/agentviz/tools/emsdk-env.sh; fi
STRATA_TARBALL="$tarball" STRATA_SHA256="$STRATA_SHA256" .strata/kit/build-kit.sh scene/kit.conf
