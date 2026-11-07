#!/usr/bin/env bash
# A radio module with a plain crystal (MM_EMU_NO_TCXO: the chip refuses the TCXO command) starts again without the TCXO,
# on 433 MHz too, and keeps working after a settings change.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
mkdir -p "$d/A";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/A/"
printf '433.650\t250.00\t11\t5\t10\tAlpha\t2\n' > "$d/A/sd/mattermesh/radio.conf"
printf '5000 serial radio config 433.650 62.5 8 8 10\n5500 serial radio status\n6000 quit\n' > "$d/a.emu"
MM_EMU_NO_TCXO=1 timeout 40 artifacts/emu-plus --headless --data "$d/A" --port-offset 2100 --script "$d/a.emu" < /dev/null > "$d/a.log" 2>&1 || true
fail(){ echo "FAIL: $1";grep -v '^\[I\]' "$d/a.log"|tail -15;exit 1; }
grep -q "RADIO_BOOT ready=1 frequency=433.650 band=433 .*error=0" "$d/a.log"||fail "the radio did not start without the TCXO"
grep -q "RADIO_TCXO 0.0" "$d/a.log"||fail "the TCXO setting did not fall back to 0"
grep -q "RADIO_STATUS ready=1 .*error=0 .*frequency=433.650 bw=62.50 sf=8 cr=8" "$d/a.log"||fail "no radio after the settings change"
echo "PASS: no TCXO - a plain crystal module starts on 433 MHz and takes a settings change"
