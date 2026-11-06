#!/usr/bin/env bash
set -euo pipefail
CPP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$CPP_DIR/../../../../../.." && pwd)"
FOG_DIR="${FOG_DIR:-$ROOT_DIR/examples/fog}"
ASSET_DIR="${FOG_ASSET_DIR:-$ROOT_DIR/assets}"
OUTPUT="${1:?usage: prepare-assets.sh OUTPUT_ASSETS_DIRECTORY}"
runtime="$OUTPUT/fog-runtime"
[[ -f "$ASSET_DIR/scene.json" ]] || { echo 'error: missing scene.json in FOG_ASSET_DIR' >&2; exit 1; }
mkdir -p "$runtime/test_data/imgs"
# Mobile by default; FOG_MOBILE=0 builds the desktop shader variant for A/B.
FOG_MOBILE="${FOG_MOBILE:-1}" bash "$FOG_DIR/build_shaders.sh" "$runtime/fog/shaders"
cp "$ASSET_DIR/scene.json" "$runtime/scene.json"
cp "$CPP_DIR/config.android.json" "$runtime/config.fog.json"
# Only known runtime resources, never logs, desktop ini files or the full test_data tree.
for resource in dragon.obj wqy-microhei.ttf; do
    cp "$ASSET_DIR/test_data/$resource" "$runtime/test_data/$resource"
done
cp "$ASSET_DIR/test_data/imgs/ice.png" "$runtime/test_data/imgs/ice.png"
