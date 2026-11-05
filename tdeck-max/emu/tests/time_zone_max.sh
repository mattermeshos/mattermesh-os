#!/usr/bin/env bash
# Time zone (Settings > Date / time and the welcome): keys, touch and BACK pick a zone, the clock follows it at once,
# UTC does not move, and the choice survives a restart.
set -euo pipefail
cd "$(dirname "$0")/../.."
data=$(mktemp -d)
trap 'rm -rf "$data"' EXIT
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$data/"
# Keys: k up, j down, code 21 ENTER (emu/devices/script.cpp letter table). Rows: list row 0 at y 73, zone row at y 204.
cat > "$data/script.emu" <<'SCRIPT'
8000 serial ui unlock
9000 serial time zone
9500 serial section 16
10500 shot
11000 type k
12000 shot
12500 tap 21
13500 shot
14000 type j
15000 serial time zone
15500 click 120 73
16500 serial time zone
17000 shot
17500 click 120 204
18500 click 14 20
19500 serial time zone
20000 shot
20500 serial time zone 3
21000 serial welcome start
22000 click 176 297
23000 type j
24000 click 176 297
25000 click 176 297
26000 click 176 297
27000 click 176 297
28000 click 176 297
29000 serial welcome status
29500 type j
30500 serial time zone
31000 shot
31500 click 176 297
32500 shot
33000 click 176 297
34500 serial time zone
35000 quit
SCRIPT
MM_EMU_LICENSED=1 timeout 90 artifacts/emu-max --headless --air off --data "$data" --script "$data/script.emu" < /dev/null > "$data/console.log" 2>&1
printf '6000 serial time zone\n7000 quit\n' > "$data/again.emu"
MM_EMU_LICENSED=1 timeout 60 artifacts/emu-max --headless --air off --data "$data" --script "$data/again.emu" < /dev/null > "$data/again.log" 2>&1
fail(){ echo "FAIL: $1"; grep -E 'TIME_|WELCOME|TOUCH_TAP' "$data/console.log" | tail -30; exit 1; }
zones=$(grep -o 'TIME_ZONE index=.*' "$data/console.log")
echo "$zones"
# check N INDEX OFFSET_MINUTES[|SUMMER] PICKING: the Nth reply has that zone, local - UTC offset and list state
check(){ python3 - "$(sed -n "${1}p" <<<"$zones")" "$2" "$3" "$4" <<'PY' || fail "$5"
import re,sys
line,index,offsets,picking=sys.argv[1],int(sys.argv[2]),[int(o)%1440 for o in sys.argv[3].split('|')],sys.argv[4]
m=re.search(r'index=(\d+) .* local=(\d+):(\d+) utc=(\d+):(\d+) picking=(\d)',line);assert m,line
i,lh,lm,uh,um,p=m.groups();d=(int(lh)*60+int(lm)-int(uh)*60-int(um))%1440
assert int(i)==index,(line,index);assert d in offsets,(line,d,offsets)
assert picking=='-' or p==picking,(line,picking)
PY
}
# The emulator clock is the PC's: zones with summer time accept both offsets.
check 1 12 '60|120' 0 'default Central Europe'
check 2 13 120 1 'keys: zone row, ENTER opens the list, down picks South Africa'
check 3 10 0 0 'touch on the first row picks UTC and closes the list'
check 4 10 0 0 'BACK closes the list and keeps the zone'
check 5 3 '-420|-360' 0 'serial: US Mountain'
check 6 4 -420 - 'welcome step 6: down picks Arizona'
check 7 4 -420 - 'welcome finished with Arizona'
grep -q 'WELCOME_STATUS active=1 step=6' "$data/console.log" || fail 'welcome reaches the time zone step'
grep -q 'WELCOME_DONE' "$data/console.log" || fail 'welcome FINISH'
grep -q 'TIME_ZONE index=4 .*name=Arizona' "$data/again.log" || { tail -20 "$data/again.log"; fail 'zone kept after a restart'; }
mkdir -p artifacts/time-zone-shots && cp "$data"/screenshot-*.ppm artifacts/time-zone-shots/ 2>/dev/null || true
echo 'PASS: time zone row, list by keys / touch / BACK, clock follows at once, welcome step, kept after restart'
