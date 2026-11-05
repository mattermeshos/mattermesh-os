#!/usr/bin/env bash
# Chat copy / paste by touch on the Max: a finger held on a message shows COPY over the chat, a tap on it copies the
# whole message; held on the empty composer it shows PASTE, which inserts the text; with a draft it shows COPY ALL and
# PASTE; a tap elsewhere closes the buttons. Holds start after the 2 s "copied" notice: a top banner that comes or
# goes during a touch cancels it (touchScene), as on the device.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/"
cat > "$d/a.emu" <<'S'
9000 tap 21
10500 tap 21
12500 type Ciao copia questo
15000 tap 21
19000 touch 170 64 down
20500 touch 170 64 up
21500 serial clipboard status
22000 shot
22500 click 120 95
23500 serial clipboard status
26000 touch 100 300 down
27500 touch 100 300 up
28500 serial clipboard status
29000 click 120 265
30000 serial composer status
33000 touch 100 300 down
34500 touch 100 300 up
35500 serial clipboard status
36000 shot
36500 click 120 180
37500 serial clipboard status
38000 quit
S
timeout 60 artifacts/emu-max --headless --data "$d" --air off --script "$d/a.emu" < /dev/null > "$d/a.log" 2>&1||true
grep -E 'CLIPBOARD|COMPOSER_STATUS' "$d/a.log" > "$d/seen"||true;cat "$d/seen"
[ -n "${EMU_SHOTS:-}" ]&&cp "$d"/screenshot-*.ppm "$EMU_SHOTS"/ 2>/dev/null||true
fail(){ echo "FAIL: $1";exit 1; }
l(){ sed -n "$1p" "$d/seen"; }
l 1|grep -q 'bytes=0 menu=1'||fail "holding a message did not show COPY"
l 2|grep -q 'bytes=17 menu=0'||fail "COPY did not copy the whole message"
l 3|grep -q 'menu=2'||fail "holding the composer did not show PASTE"
l 4|grep -q 'COMPOSER_STATUS bytes=17 '||fail "PASTE did not insert the text"
l 5|grep -q 'menu=2'||fail "holding a draft did not show COPY ALL / PASTE"
l 6|grep -q 'menu=0'||fail "a tap elsewhere did not close the buttons"
echo "PASS: Max chat copy / paste - hold a message -> COPY, hold the composer -> PASTE / COPY ALL, tap elsewhere closes"
