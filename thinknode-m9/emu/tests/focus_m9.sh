#!/usr/bin/env bash
# M9 D-pad: row pages and the focus ring.
# Row pages (Wi-Fi, Sound, Power, LoRa): up / down select a row, left / right change its value, OK flips / runs.
# Maps: OK opens its settings as rows, right on Zoom zooms in. Settings: three downs + OK = Bluetooth (LoRa, Companion link, Wi-Fi above it).
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/"
cat > "$d/f.emu" <<'S'
4000 serial open 4
5000 pressclick
5800 serial wifi status
6000 serial open 21
6800 serial audio status
7000 roll 0 1
7200 roll 1 0
7600 serial audio status
8000 serial open 2
8800 roll 1 0
9400 serial status
9600 serial open 9
10300 serial map status
10500 pressclick
11000 roll 1 0
11400 serial map status
11600 serial open 19
12300 roll 0 1
12550 roll 0 1
12800 roll 0 1
13100 pressclick
13700 serial status
14200 quit
S
timeout 40 artifacts/emu-m9 --headless --data "$d" --air off --script "$d/f.emu" </dev/null > "$d/f.log" 2>&1||true
fail(){ echo "FAIL: $1";grep -E 'MAP_STATUS|WIFI_STATUS|AUDIO_STATUS|STATUS demo' "$d/f.log"|cut -c1-140|tail -8;exit 1; }
grep '^WIFI_STATUS' "$d/f.log"|head -1|grep -q 'on=0'||fail "Wi-Fi: OK on the Wi-Fi row did not turn it off"
v(){ grep -i '^AUDIO_STATUS' "$d/f.log"|sed -n "$1p"|grep -o 'volume=[0-9]*'|tr -dc 0-9; }
[ -n "$(v 1)" ]&&[ "$(v 1)" -gt 0 ]&&[ "$(v 1)" -lt 60 ]&&[ "$(v 2)" = 100 ]||fail "Sound: down then right did not raise the volume from Low to High"
grep '^STATUS demo' "$d/f.log"|sed -n 2p|grep -q 'radio_device=1\|section=2'||fail "LoRa: page lost"
z(){ grep '^MAP_STATUS' "$d/f.log"|sed -n "$1p"|grep -o 'zoom=[0-9]*'|tr -dc 0-9; }
[ -n "$(z 1)" ]&&[ "$(z 2)" = "$(( $(z 1)+1 ))" ]||fail "Maps: OK then right (Zoom row) did not zoom in"
grep '^STATUS demo' "$d/f.log"|tail -1|grep -q ' section=14 '||fail "Settings: three downs and OK did not open Bluetooth"
grep -q 'M9_KEY unknown' "$d/f.log"&&fail "a key code went unhandled"
echo "PASS: M9 D-pad - Wi-Fi row OK, Sound volume Low -> High, Maps settings rows, Settings rows"
