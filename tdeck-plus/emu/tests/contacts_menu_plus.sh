#!/usr/bin/env bash
# Contacts menu with real adverts on the air. Bravo has a full-size book of old contacts and advertises to Alpha.
# Add new off: Alpha hears Bravo but does not save or list it; on: Bravo is listed live. Replace oldest at a full book:
# the oldest contact without a chat goes, the next advert gets the slot. Delete oldest keeps the open chat, Delete all
# empties the book, and contacts.tsv and both settings survive a reboot.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((7900+RANDOM%500))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
for n in Alpha Bravo;do mkdir -p "$d/$n";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/$n/";done
python3 - "$d/Alpha/sd/mattermesh/contacts.tsv" <<'P'
import sys,time,hashlib
now=int(time.time())
with open(sys.argv[1],'w') as f:
    for i in range(1024):
        f.write(f"{hashlib.sha256(str(i).encode()).hexdigest()}\tOld-{i:04d}\t1\t255\t\t{now-(1024-i)*3600}\t0\t0\n")
P
cat > "$d/alpha.emu" <<'S'
3000 serial section 1
3400 serial contacts menu status
3600 click 305 34
4000 click 160 90
4400 serial contacts menu status
4600 tap 22
9000 serial contacts status
9100 serial contacts menu status
9300 serial section 1
9600 click 40 62
10200 serial section 1
10600 click 305 34
11000 click 160 90
11300 click 160 130
11600 serial contacts menu status
11800 click 8 230
16000 serial contacts menu status
16200 serial contacts rows
21000 serial contacts menu status
21200 serial contacts rows
21500 click 305 34
21900 click 160 170
22300 serial contacts menu status
22500 click 66 207
23000 serial contacts menu status
23100 click 305 34
23400 roll 0 -1
23700 pressclick
23840 pressclick
24100 serial contacts menu status
24300 click 305 34
24700 click 160 210
25150 click 66 207
25500 serial contacts menu status
25800 click 305 34
26100 click 160 90
26400 click 8 230
31000 serial contacts menu status
31300 serial section 1
31600 click 305 34
32000 click 160 90
32300 click 8 230
52000 serial contacts menu status
52300 serial contacts rows
53300 quit
S
cat > "$d/bravo.emu" <<'S'
6500 serial radio advert
14000 serial radio advert
19000 serial radio advert
28000 serial radio advert
45500 serial ui unlock
46000 serial radio advert
52500 quit
S
artifacts/emu-air --port $port > "$d/broker.out" 2>&1 &
sleep 0.5
timeout 75 artifacts/emu-plus --headless --data "$d/Alpha" --name Alpha --air $port --port-offset 1700 --script "$d/alpha.emu" < /dev/null > "$d/alpha.log" 2>&1 &
timeout 75 artifacts/emu-plus --headless --data "$d/Bravo" --name Bravo --air $port --port-offset 1800 --script "$d/bravo.emu" < /dev/null > "$d/bravo.log" 2>&1 &
wait %2 2>/dev/null||true;wait %3 2>/dev/null||true
printf '3000 serial contacts menu status\n3200 quit\n' > "$d/again.emu"
timeout 15 artifacts/emu-plus --headless --data "$d/Alpha" --air off --script "$d/again.emu" < /dev/null > "$d/again.log" 2>&1||true
grep -E 'CONTACT_MENU|CONTACTS_STATUS|CONTACT_ROWS|type=1 live=1' "$d/alpha.log" > "$d/seen"||true
cat "$d/seen";grep CONTACT_MENU "$d/again.log"
fail(){ echo "FAIL: $1";exit 1; }
m(){ grep CONTACT_MENU "$d/seen"|sed -n "$1p"; }
m 1|grep -q 'add_new=1 replace=0 saved=1024'||fail "defaults"
m 3|grep -q 'add_new=0 replace=0 saved=1024'||fail "Add new did not switch off"
m 5|grep -q 'saved=1023'||fail "Replace oldest did not free a slot"
m 6|grep -q 'saved=1024'||fail "the next advert did not take the freed slot"
grep -q 'CONTACT_ROW index=[0-9]* lat=[-0-9.]* type=1 live=1' "$d/seen"||fail "Bravo not listed live"
m 8|grep -q 'saved=924'||fail "Delete oldest did not remove 100"
m 9|grep -q 'dialog=0 .*saved=924'||fail "a double press on Delete all deleted"
m 10|grep -q 'saved=0 '||fail "Delete all left contacts"
m 11|grep -q 'add_new=0 replace=1 saved=0 '||fail "a node was saved with Add new off"
m 12|grep -q 'add_new=1 replace=1 saved=1 '||fail "Add new on did not save the node"
grep -q 'CONTACT_MENU .*add_new=1 replace=1 saved=1 ' "$d/again.log"||fail "settings or book lost at reboot"
[ "$(wc -l < "$d/Alpha/sd/mattermesh/contacts.tsv")" = 1 ]||fail "contacts.tsv not rewritten"
echo "PASS: Contacts menu - Add new, Replace oldest with a chat protected, Delete oldest, Delete all, saved across a reboot"
