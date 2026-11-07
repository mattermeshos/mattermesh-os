#!/usr/bin/env bash
# ThinkNode M9 prototype: an M9 (LR1110 model, keyboard MCU, D-pad) and a T-Deck Plus (SX1262 model) on one air.
# Both advertise; the M9 opens the Public chat with the D-pad keys, types with the M9 keyboard and sends; the Plus must hear it
# and answer, and the M9 must hear that. PLUS_EMU = a built emu-plus (default artifacts/emu-plus).
set -euo pipefail
cd "$(dirname "$0")/../.."
plus=${PLUS_EMU:-artifacts/emu-plus};[ -x "$plus" ]||plus=../tdeck-plus-free/artifacts/emu-plus  # the M9 tree builds no emu-plus
d=$(mktemp -d);port=$((7900+RANDOM%500))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;[ -n "${KEEP:-}" ]&&echo "kept $d"||rm -rf "$d"; };trap cleanup EXIT
for n in Emme Piu;do mkdir -p "$d/$n";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/$n/";printf '869.618\t62.50\t8\t8\t22\t%s\t2\n' "$n" > "$d/$n/sd/mattermesh/radio.conf";rm -f "$d/$n/sd/mattermesh/identity.id";done
cat > "$d/m9.emu" <<'S'
5000 serial radio advert
9500 tap 21
10500 tap 21
11800 type Ciao da M9
13800 tap 21
15000 shot
21000 shot
23000 serial ui lock
24500 shot
25000 quit
S
cat > "$d/plus.emu" <<'S'
6500 serial radio advert
9500 tap 21
10500 tap 21
17000 type Ciao Plus
19000 tap 21
24500 quit
S
artifacts/emu-air --port $port --log "$d/air.log" > "$d/broker.out" 2>&1 &
sleep 0.5
timeout 60 artifacts/emu-m9 --headless --data "$d/Emme" --air $port --port-offset 1400 --script "$d/m9.emu" < /dev/null > "$d/m9.log" 2>&1 &
timeout 60 "$plus" --headless --data "$d/Piu" --air $port --port-offset 1500 --script "$d/plus.emu" < /dev/null > "$d/plus.log" 2>&1
wait %2 2>/dev/null||true
fail(){ echo "FAIL: $1";for f in air.log m9.log plus.log;do echo "--- $f";grep -v '^\[I\]' "$d/$f"|tail -8|cut -c1-200;done;exit 1; }
grep -q "RADIO_BOOT ready=1.*chip=LR1110" "$d/m9.log"||fail "M9 radio not ready"
grep -q "RADIO_BOOT ready=1" "$d/plus.log"||fail "Plus radio not ready"
[ "$(grep -c 'sender=Emme receivers=Piu:-70/10/ok' "$d/air.log")" -ge 2 ]||fail "the Plus did not hear the M9's advert and message"
[ "$(grep -c 'sender=Piu receivers=Emme:-70/10/ok' "$d/air.log")" -ge 2 ]||fail "the M9 did not hear the Plus's advert and message"
cat "$d/air.log"|cut -c1-110|head -8
[ -n "${KEEP:-}" ]&&cp "$d"/Emme/screenshot-*.ppm "${KEEP}/" 2>/dev/null
echo "PASS: M9 and Plus on one air - LR1110 up, adverts both ways, typed Public messages both ways"
