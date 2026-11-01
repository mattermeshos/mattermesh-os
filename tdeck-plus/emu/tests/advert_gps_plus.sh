#!/usr/bin/env bash
# The GPS switch beside ADVERT: off (default) Bravo gets no position, on it gets one (the emulated GPS fix); the choice is saved.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((7900+RANDOM%500))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
for n in Alpha Bravo;do mkdir -p "$d/$n";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/$n/";printf '869.618\t62.50\t8\t8\t22\t%s\t1\n' "$n" > "$d/$n/sd/mattermesh/radio.conf";done
cat > "$d/alpha.emu" <<'S'
3000 serial gps set 44.3 8.4
4000 serial radio advert
12000 serial radio advert gps on
12500 serial radio advert
21000 quit
S
cat > "$d/bravo.emu" <<'S'
10000 serial contacts rows
19500 serial contacts rows
21000 quit
S
artifacts/emu-air --port $port > "$d/broker.out" 2>&1 &
sleep 0.5
timeout 40 artifacts/emu-plus --headless --data "$d/Alpha" --name Alpha --air $port --port-offset 2300 --script "$d/alpha.emu" < /dev/null > "$d/alpha.log" 2>&1 &
timeout 40 artifacts/emu-plus --headless --data "$d/Bravo" --name Bravo --air $port --port-offset 2400 --script "$d/bravo.emu" < /dev/null > "$d/bravo.log" 2>&1 &
wait %2 2>/dev/null||true;wait %3 2>/dev/null||true
printf '3000 serial radio advert\n4000 quit\n' > "$d/again.emu"
fail(){ echo "FAIL: $1";grep -a -E 'CONTACT_ROW|ADVERT|GPS_SET' "$d/alpha.log" "$d/bravo.log"|cut -c1-150|tail -8;exit 1; }
grep -aq 'GPS_SET ok=1' "$d/alpha.log"||fail "position not set"
grep -a 'CONTACT_ROW index=0' "$d/bravo.log"|sed -n 1p|grep -q 'lat=0.000000'||fail "a position with the switch off"
grep -a 'CONTACT_ROW index=0' "$d/bravo.log"|sed -n 2p|grep -q 'lat=[1-9]'||fail "no position with the switch on"
timeout 15 artifacts/emu-plus --headless --data "$d/Alpha" --air off --script "$d/again.emu" < /dev/null > "$d/again.log" 2>&1||true
python3 -c "import json,sys;t=open('$d/Alpha/nvs.json').read();sys.exit(0 if 'mmadvert' in t else 1)"||fail "the choice was not saved"
echo 'PASS: advert GPS - no position by default, position with the switch on, choice saved'
