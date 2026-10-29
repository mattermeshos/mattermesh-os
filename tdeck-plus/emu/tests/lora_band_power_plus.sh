#!/usr/bin/env bash
# LoRa page band buttons and the TX power draft: 868 at 14 dBm -> 900 keeps 14, -> 433 takes 10 (band default),
# -> back to 868 (the saved band) restores 14. Read from the page with the trackball/keys ('b' = next band).
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);cleanup(){ rm -rf "$d"; };trap cleanup EXIT
mkdir -p "$d/A";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/A/"
printf '869.618\t62.50\t8\t8\t14\tAlpha\t2\n' > "$d/A/sd/mattermesh/radio.conf"
cat > "$d/a.emu" <<'S'
5000 serial section 2
6000 serial ui radio draft
6500 type b
7000 serial ui radio draft
7500 type b
8000 serial ui radio draft
8500 type b
9000 serial ui radio draft
10000 quit
S
timeout 40 artifacts/emu-plus --headless --data "$d/A" --port-offset 1500 --script "$d/a.emu" < /dev/null > "$d/a.log" 2>&1||true
got=$(grep -o 'RADIO_DRAFT loaded=1 band=[0-9]* power=[0-9]*' "$d/a.log"|tr '\n' ' ')
want='RADIO_DRAFT loaded=1 band=868 power=14 RADIO_DRAFT loaded=1 band=900 power=14 RADIO_DRAFT loaded=1 band=433 power=10 RADIO_DRAFT loaded=1 band=868 power=14 '
[ "$got" = "$want" ]||{ echo "FAIL: band power drafts: $got";grep -v '^\[I\]' "$d/a.log"|tail -15;exit 1; }
echo "PASS: LoRa band buttons - 868 at 14 dBm, 900 keeps 14, 433 takes 10, back to 868 restores 14"
