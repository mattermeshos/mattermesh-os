#!/usr/bin/env bash
# Contacts menu with real adverts on the air. Bravo has a full-size book of old contacts and advertises to Alpha.
# Add new off: Alpha hears Bravo but does not save or list it; on: Bravo is listed live. Replace oldest at a full book:
# the oldest contact without a chat goes, the next advert gets the slot. Delete oldest keeps the open chat, Delete all
# empties the book, and contacts.tsv and both settings survive a reboot.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((7900+RANDOM%500))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
for n in Alpha Bravo;do mkdir -p "$d/$n";cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/$n/";done
python3 - "$d/Alpha/sd/mattermesh/contacts.tsv" <<'P'
import sys,time,hashlib
now=int(time.time())
with open(sys.argv[1],'w') as f:
    for i in range(1024):
        f.write(f"{hashlib.sha256(str(i).encode()).hexdigest()}\tOld-{i:04d}\t1\t255\t\t{now-(1024-i)*3600}\t0\t0\n")
P
cat > "$d/alpha.emu" <<'S'
9000 serial section 1
10200 serial contacts menu status
10800 click 225 34
12000 click 120 98
13200 serial contacts menu status
27000 serial contacts status
27300 serial contacts menu status
27900 serial section 1
28800 click 60 62
30600 serial section 1
31800 click 225 34
33000 click 120 98
33900 click 120 138
34800 serial contacts menu status
35400 click 4 300
48000 serial contacts menu status
48600 serial contacts rows
63000 serial contacts menu status
63600 serial contacts rows
64500 click 225 34
65700 click 120 178
66900 serial contacts menu status
67500 click 66 238
69000 serial contacts menu status
69300 click 225 34
70200 serial key k
71100 serial key ENTER
71520 serial key ENTER
72300 serial contacts menu status
72900 click 225 34
74100 click 120 218
75450 click 66 238
76500 serial contacts menu status
77400 click 225 34
78300 click 120 98
79200 click 4 300
93000 serial contacts menu status
93900 serial section 1
94800 click 225 34
96000 click 120 98
96900 click 4 300
156000 serial contacts menu status
156900 serial contacts rows
159900 quit
S
cat > "$d/bravo.emu" <<'S'
3000 serial power display 0
19500 serial radio advert
42000 serial radio advert
57000 serial radio advert
84000 serial radio advert
136500 serial power wake
138000 serial radio advert
157500 quit
S
artifacts/emu-air --port $port > "$d/broker.out" 2>&1 &
sleep 0.5
timeout 200 artifacts/emu-max --headless --data "$d/Alpha" --name Alpha --air $port --port-offset 1700 --script "$d/alpha.emu" < /dev/null > "$d/alpha.log" 2>&1 &
timeout 200 artifacts/emu-max --headless --data "$d/Bravo" --name Bravo --air $port --port-offset 1800 --script "$d/bravo.emu" < /dev/null > "$d/bravo.log" 2>&1 &
wait %2 2>/dev/null||true;wait %3 2>/dev/null||true
printf '10000 serial contacts menu status\n11000 quit\n' > "$d/again.emu"
timeout 30 artifacts/emu-max --headless --data "$d/Alpha" --air off --script "$d/again.emu" < /dev/null > "$d/again.log" 2>&1||true
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
