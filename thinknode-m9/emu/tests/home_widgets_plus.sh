#!/usr/bin/env bash
# Settings > Home widgets > Beside clock: Sunset by default with sunrise / sunset from the weather position (Cervinia,
# late October: about 07:0x and 17:2x local), a tap on the right arrow and the trackball browse the carousel (down:
# next, up: back), a click uses the focused widget, the choice survives a reboot, Calendar has a week number.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);cleanup(){ rm -rf "$d"; };trap cleanup EXIT
mkdir -p "$d/B";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/B/"
cat > "$d/one.emu" <<'S'
3000 serial weather location 45.93 7.63
3500 serial home widget status
4000 serial section 36
4400 pressclick
4800 click 290 133
5400 roll 0 1
6000 roll 0 1
6600 roll 0 -1
6800 serial home widget status
7000 pressclick
7600 serial home widget status
8000 serial home widget set 10
8300 serial home widget status
8600 serial home widget set 4
9000 quit
S
cat > "$d/two.emu" <<'S'
3000 serial home widget status
3500 quit
S
timeout 30 artifacts/emu-plus --headless --data "$d/B" --script "$d/one.emu" < /dev/null > "$d/one.log" 2>&1||true
timeout 30 artifacts/emu-plus --headless --data "$d/B" --script "$d/two.emu" < /dev/null > "$d/two.log" 2>&1||true
fail(){ echo "FAIL: $1";grep -E 'HOME_WIDGET' "$d/one.log" "$d/two.log";exit 1; }
st=$(grep "^HOME_WIDGET id=" "$d/one.log"||true)
[ "$(echo "$st"|wc -l)" -ge 4 ]||fail "no home widget status"
echo "$st"|sed -n 1p|grep -q 'id=5 .*sun=1 rise=0[6-8]:[0-9][0-9] set=1[6-8]:[0-9][0-9] next=[0-9].*name="Sunset"'||fail "Sunset is not the default or its sunrise/sunset from the weather position are off"
echo "$st"|sed -n 2p|grep -q "id=5 focus=7 "||fail "the arrow tap and the trackball did not browse the carousel"
echo "$st"|sed -n 3p|grep -q 'id=7 focus=7 .*name="Last weather"'||fail "a click did not use the focused widget"
echo "$st"|sed -n 4p|grep -q "id=10 .*week=[1-9]"||fail "Calendar has no week number"
grep -q '^HOME_WIDGET id=4 .*name="Day arc"' "$d/two.log"||fail "the choice did not survive a reboot"
echo "PASS: Home widgets - Sunset by default with plausible sun times from the weather position, arrow tap and trackball browse, click to use, saved across a reboot, week number"
