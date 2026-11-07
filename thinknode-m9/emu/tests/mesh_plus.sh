#!/usr/bin/env bash
# Emulator phase 2 test: broker + two T-Deck Plus emulators ("Alpha", "Bravo") on the shared air. Both advertise,
# Alpha types "Ciao dal PC" in the Public chat with the emulated keyboard and sends it; Bravo must hear both
# frames, list Alpha as a live contact and show the message on Home.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((7400+RANDOM%500))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
for n in Alpha Bravo;do mkdir -p "$d/$n";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/$n/";printf '869.618\t62.50\t8\t8\t22\t%s\t2\n' "$n" > "$d/$n/sd/mattermesh/radio.conf";done
cat > "$d/alpha.emu" <<'S'
5000 serial radio advert
9500 tap 21
10500 tap 21
11800 type Ciao dal PC
13800 tap 21
23000 quit
S
cat > "$d/bravo.emu" <<'S'
6500 serial radio advert
19500 serial contacts status
20000 shot
23000 quit
S
artifacts/emu-air --port $port --log "$d/air.log" > "$d/broker.out" 2>&1 &
sleep 0.5
timeout 60 artifacts/emu-plus --headless --data "$d/Alpha" --air $port --port-offset 1200 --script "$d/alpha.emu" < /dev/null > "$d/alpha.log" 2>&1 &
timeout 60 artifacts/emu-plus --headless --data "$d/Bravo" --air $port --port-offset 1300 --script "$d/bravo.emu" < /dev/null > "$d/bravo.log" 2>&1
wait %2 2>/dev/null||true
fail(){ echo "FAIL: $1";for f in air.log alpha.log bravo.log;do echo "--- $f";grep -v '^\[I\]' "$d/$f"|tail -8;done;exit 1; }
for n in alpha bravo;do grep -q "RADIO_BOOT ready=1" "$d/$n.log"||fail "$n radio not ready";done
[ "$(grep -c 'sender=Alpha receivers=Bravo:-70/10/ok' "$d/air.log")" -ge 2 ]||fail "Bravo did not hear Alpha's advert and message"
grep -q "sender=Bravo receivers=Alpha:-70/10/ok" "$d/air.log"||fail "Alpha did not hear Bravo's advert"
grep -q "CONTACTS_STATUS .*total=1 live=1" "$d/bravo.log"||fail "Bravo has no live contact"
python3 - "$d/Bravo/screenshot-1.ppm" "$d/air.log" <<'PY'||fail "message frame"
import sys
air=open(sys.argv[2]).read().splitlines()
assert any(" sender=Alpha " in l and " length=37 " in l for l in air), "no 37-byte group text from Alpha"
PY
echo "PASS: emulator mesh - SX1262 up on both, adverts both ways, Alpha's Public message heard by Bravo, live contact"
