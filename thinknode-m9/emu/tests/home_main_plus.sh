#!/usr/bin/env bash
# Settings > Home widgets > Main area: Chats by default; the Home widgets page moves to its second row with the
# trackball and opens the Main area carousel, which browses and uses a widget with a click; the choice and the note
# survive a reboot; Position has UTM, GPS sky the satellites of the emulated GPS.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);cleanup(){ rm -rf "$d"; };trap cleanup EXIT
mkdir -p "$d/B";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/B/"
cat > "$d/one.emu" <<'S'
3000 serial home main status
3300 serial note set Hut by 15:00
3600 serial section 36
4000 roll 0 1
4500 pressclick
5000 roll 0 1
5300 serial home main status
5500 roll 0 1
5800 serial home main status
6000 roll 0 -1
6400 serial home main status
6600 pressclick
7000 serial home main status
9000 serial home main set 4
9300 serial home main status
9600 serial home main set 5
9900 serial home main status
10200 serial home main set 9
10400 serial home
10700 click 160 120
11000 serial banner status
11300 type ok
11600 tap 21
12000 serial home main status
12300 quit
S
cat > "$d/two.emu" <<'S'
3000 serial home main status
3500 quit
S
timeout 30 artifacts/emu-plus --headless --data "$d/B" --script "$d/one.emu" < /dev/null > "$d/one.log" 2>&1||true
timeout 30 artifacts/emu-plus --headless --data "$d/B" --script "$d/two.emu" < /dev/null > "$d/two.log" 2>&1||true
fail(){ echo "FAIL: $1";grep -E 'HOME_MAIN|NOTE_SET' "$d/one.log" "$d/two.log";exit 1; }
st=$(grep "^HOME_MAIN id=" "$d/one.log"||true)
[ "$(echo "$st"|wc -l)" -ge 7 ]||fail "no main area status"
echo "$st"|sed -n 1p|grep -q 'id=0 .*name="Chats"'||fail "Chats is not the default"
echo "$st"|sed -n 2p|grep -q "id=0 focus=1 "||fail "the trackball did not reach the Main area carousel"
echo "$st"|sed -n 3p|grep -q "id=0 focus=2 "||fail "a second roll did not move to the next widget"
echo "$st"|sed -n 4p|grep -q "id=0 focus=1 "||fail "rolling up did not move back"
echo "$st"|sed -n 5p|grep -q 'id=1 focus=1 .*name="Timeline"'||fail "a click did not use the focused widget"
echo "$st"|sed -n 6p|grep -q 'id=4 .*utm=32T [0-9]* [0-9]* '||fail "Position has no UTM from the emulated fix"
echo "$st"|sed -n 7p|grep -q 'id=5 .*sky=[1-9]'||fail "GPS sky has no satellites"
grep -q "^BANNER_STATUS .* screen=4 section=39 " "$d/one.log"||fail "a tap on the Note widget did not open its editor"
grep -q "NOTE_SET ok=1 bytes=12" "$d/one.log"||fail "the note was not saved"
grep -q '^HOME_MAIN id=9 .*note=14 name="Note"' "$d/one.log"||fail "typing in the note editor and ENTER did not save it"
grep -q '^HOME_MAIN id=9 .*note=14 name="Note"' "$d/two.log"||fail "the choice or the note did not survive a reboot"
echo "PASS: Home main area - Chats by default, trackball to the carousel and each roll, click to use, a tap on Note opens its editor, typing + ENTER saves it, Position UTM, GPS sky satellites, note and choice saved across a reboot"
