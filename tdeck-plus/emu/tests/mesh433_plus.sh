#!/usr/bin/env bash
# 433 MHz band (T-Deck Plus units built for 433): Alpha and Bravo on EU 433MHz (Narrow) at 10 dBm exchange adverts
# and a Public message as in mesh_plus.sh; Charlie stays on 869.618 MHz and must decode none of it (the broker
# delivers every frame; the emulated SX1262 drops the ones off its frequency).
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((7400+RANDOM%500))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
for n in Alpha Bravo Charlie;do mkdir -p "$d/$n";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/$n/";done
for n in Alpha Bravo;do printf '433.650\t62.50\t8\t8\t10\t%s\t2\n' "$n" > "$d/$n/sd/mattermesh/radio.conf";done
printf '869.618\t62.50\t8\t8\t22\tCharlie\t2\n' > "$d/Charlie/sd/mattermesh/radio.conf"
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
cat > "$d/charlie.emu" <<'S'
19500 serial contacts status
19800 serial radio status
23000 quit
S
artifacts/emu-air --port $port --log "$d/air.log" > "$d/broker.out" 2>&1 &
sleep 0.5
timeout 60 artifacts/emu-plus --headless --data "$d/Alpha" --air $port --port-offset 1200 --script "$d/alpha.emu" < /dev/null > "$d/alpha.log" 2>&1 &
timeout 60 artifacts/emu-plus --headless --data "$d/Charlie" --air $port --port-offset 1400 --script "$d/charlie.emu" < /dev/null > "$d/charlie.log" 2>&1 &
timeout 60 artifacts/emu-plus --headless --data "$d/Bravo" --air $port --port-offset 1300 --script "$d/bravo.emu" < /dev/null > "$d/bravo.log" 2>&1
wait %2 %3 2>/dev/null||true
fail(){ echo "FAIL: $1";for f in air.log alpha.log bravo.log charlie.log;do echo "--- $f";grep -v '^\[I\]' "$d/$f"|tail -8;done;exit 1; }
for n in alpha bravo;do grep -q "RADIO_BOOT ready=1 frequency=433.650 band=433 .*power=10 " "$d/$n.log"||fail "$n radio not ready on 433.650 at 10 dBm";done
grep -q "RADIO_BOOT ready=1 frequency=869.618 band=868 " "$d/charlie.log"||fail "charlie radio not ready on 869.618"
[ "$(grep -c 'sender=Alpha receivers=Bravo:-70/10/ok' "$d/air.log")" -ge 2 ]||fail "Bravo did not hear Alpha's advert and message"
grep -q "sender=Bravo receivers=Alpha:-70/10/ok" "$d/air.log"||fail "Alpha did not hear Bravo's advert"
grep -q "CONTACTS_STATUS .*total=1 live=1" "$d/bravo.log"||fail "Bravo has no live contact"
grep -q "CONTACTS_STATUS .*total=0 " "$d/charlie.log"||fail "Charlie on 868 lists a 433 contact"
grep -q "RADIO_STATUS .*received=0 " "$d/charlie.log"||fail "Charlie on 868 decoded a 433 frame"
python3 - "$d/Bravo/screenshot-1.ppm" "$d/air.log" <<'PY'||fail "message frame"
import sys
air=open(sys.argv[2]).read().splitlines()
assert any(" sender=Alpha " in l and " length=37 " in l for l in air), "no 37-byte group text from Alpha"
PY
echo "PASS: emulator mesh 433 - both on 433.650 MHz at 10 dBm, adverts both ways, Alpha's Public message heard by Bravo, live contact; the 868 node hears nothing"
