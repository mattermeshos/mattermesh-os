#!/usr/bin/env bash
# An advert nobody repeats: after 30 s the T-Deck says so (the emulated air has no repeater).
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
mkdir -p "$d/A";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/A/"
printf '3000 serial ui unlock\n3200 serial section 2\n4000 serial radio advert\n48000 quit\n' > "$d/a.emu"
timeout 80 artifacts/emu-plus --headless --data "$d/A" --port-offset 2200 --script "$d/a.emu" < /dev/null > "$d/a.log" 2>&1
grep -q 'ADVERT_HEARD count=0' "$d/a.log"||{ echo 'FAIL: no answer 30 s after the advert';grep -v '^\[I\]' "$d/a.log"|tail -8;exit 1; }
echo 'PASS: advert heard - with no repeater the LoRa page says so after 30 s'
