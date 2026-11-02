#!/usr/bin/env bash
# Time zone (Settings > Date / time and the welcome): trackball, touch and BACK pick a zone, the clock follows it at
# once, UTC does not move, and the choice survives a restart.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/"
# Trackball right five times from the day box reaches the zone row (focus 5; left is BACK on the Plus); rows: zone row y 111, first list row y 70.
cat > "$d/a.emu" <<'S'
7000 serial ui unlock
7500 serial time zone
8000 serial section 16
9000 roll 5 0
9600 pressclick
10400 roll 0 1
11200 serial time zone
11600 pressclick
12200 serial time zone
12600 click 160 111
13400 click 150 70
14200 serial time zone
14600 click 160 111
15400 click 14 34
16200 serial time zone
16600 serial time zone 3
17000 serial welcome start
18000 click 248 217
18800 roll 0 1
19600 click 248 217
20400 click 248 217
21200 click 248 217
22000 click 248 217
22800 click 248 217
23600 serial welcome status
24000 roll 0 1
24800 serial time zone
25200 click 248 217
26000 click 248 217
27000 serial time zone
27500 quit
S
timeout 70 artifacts/emu-plus --headless --data "$d" --air off --script "$d/a.emu" < /dev/null > "$d/a.log" 2>&1||true
printf '4000 serial time zone\n4500 quit\n' > "$d/b.emu"
timeout 40 artifacts/emu-plus --headless --data "$d" --air off --script "$d/b.emu" < /dev/null > "$d/b.log" 2>&1||true
fail(){ echo "FAIL: $1";grep -E 'TIME_ZONE|WELCOME|TOUCH' "$d/a.log"|tail -30;exit 1; }
zones=$(grep -o 'TIME_ZONE index=.*' "$d/a.log"||true);echo "$zones"
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
check 2 13 120 1 'trackball: zone row, press opens the list, down picks South Africa'
check 3 13 120 0 'press closes the list, zone kept'
check 4 11 '0|60' 0 'touch: zone row opens the list, the first row (London) picks and closes'
check 5 11 '0|60' 0 'BACK closes the list and keeps the zone'
check 6 3 '-420|-360' 0 'serial: US Mountain'
check 7 4 -420 - 'welcome step 6: trackball down picks Arizona'
check 8 4 -420 - 'welcome finished with Arizona'
grep -q 'WELCOME_STATUS active=1 step=6' "$d/a.log" || fail 'welcome reaches the time zone step'
grep -q 'WELCOME_DONE' "$d/a.log" || fail 'welcome FINISH'
grep -q 'TIME_ZONE index=4 .*name=Arizona' "$d/b.log" || { tail -20 "$d/b.log"; fail 'zone kept after a restart'; }
echo 'PASS: time zone row, list by trackball / touch / BACK, clock follows at once, welcome step, kept after restart'
