#!/usr/bin/env bash
# Fetch the pinned third-party sources the firmware trees build against, apply the local patches and link them
# in (each tree expects ./meshcore-base next to platformio.ini; tdeck-plus/ and tdeck-max/ also expect
# ./hardware - tdeck-pro/ defines its board pins locally instead of depending on a LilyGO hardware library).
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$PWD
mkdir -p deps
fetch(){ # url commit dir
  if [ ! -d "deps/$3/.git" ]; then git clone --quiet "$1" "deps/$3"; fi
  git -C "deps/$3" fetch --quiet origin "$2" 2>/dev/null || true
  git -C "deps/$3" checkout --quiet --force "$2"
  git -C "deps/$3" clean --quiet -fdx
}
fetch https://github.com/meshcore-dev/MeshCore.git 81dbae592ffad40b4212bc0b37e82b38da08f4bc meshcore   # companion-v1.18.0
fetch https://github.com/Xinyuan-LilyGO/T-Deck-MAX.git 7b7f834746499676eb14dfc987c586b0df6aa361 t-deck-max
git -C deps/meshcore apply "$ROOT/tdeck-max/tools/patches/meshcore-login-password.patch"
git -C deps/t-deck-max apply "$ROOT/patches/lilygo-tdeck-max.patch"
for tree in tdeck-plus tdeck-max tdeck-pro; do
  ln -sfn "$ROOT/deps/meshcore" "$tree/meshcore-base"
done
for tree in tdeck-plus tdeck-max; do
  ln -sfn "$ROOT/deps/t-deck-max" "$tree/hardware"
done
echo "Dependencies ready: deps/meshcore (MeshCore companion-v1.18.0 + patch), deps/t-deck-max (LilyGO T-Deck-MAX + patch)"
