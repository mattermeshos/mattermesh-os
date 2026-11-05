#!/usr/bin/env bash
# Run a Max emulator, seeding only a new SD directory from the configured profile.
# emu/run-max.sh [DATA_DIR] [extra emu-max options...]
set -euo pipefail
cd "$(dirname "$0")/.."
data=${1:-emu/data-max}
if (( $# )); then shift; fi
if [[ ! -d "$data/sd" ]]; then
  mkdir -p "$data"
  cp -r emu/profiles/configured-max/sd "$data/"
fi
exec artifacts/emu-max --data "$data" --scale 3 "$@"
