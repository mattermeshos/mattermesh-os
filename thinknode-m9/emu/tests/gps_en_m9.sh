#!/usr/bin/env bash
# GPS enable level: a board whose GPS wants EN high (MM_EMU_GPS_EN_HIGH=1) stays silent at the default low level; the
# firmware tries the other level after 20 s without a byte, gets NMEA, saves it, and the next boot starts with it.
# A board that wants EN low (the default) gets data at once and never switches.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
fail(){ echo "FAIL: $1";grep -E 'GPS_EN|GPS_STATUS' "$d"/*.log|cut -c1-160|tail -8;exit 1; }
printf '3000 serial gps on\n8000 serial gps status\n30000 serial gps status\n31000 quit\n' > "$d/s.emu"
for b in low high;do mkdir -p "$d/$b";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/$b/";done
timeout 45 artifacts/emu-m9 --headless --data "$d/low" --air off --script "$d/s.emu" </dev/null > "$d/low.log" 2>&1||true
grep -q 'GPS_EN try' "$d/low.log"&&fail "an active-low board switched level"
grep 'GPS_STATUS' "$d/low.log"|head -1|grep -q ' data=1 '||fail "active-low board: no data at once"
MM_EMU_GPS_EN_HIGH=1 timeout 45 artifacts/emu-m9 --headless --data "$d/high" --air off --script "$d/s.emu" </dev/null > "$d/high.log" 2>&1||true
grep -q 'GPS_EN saved active_high=1' "$d/high.log"||fail "active-high board: level not found or not saved"
grep 'GPS_STATUS' "$d/high.log"|tail -1|grep -q ' fix=1 '||fail "active-high board: no fix after the switch"
MM_EMU_GPS_EN_HIGH=1 timeout 45 artifacts/emu-m9 --headless --data "$d/high" --air off --script "$d/s.emu" </dev/null > "$d/again.log" 2>&1||true
grep 'GPS_STATUS' "$d/again.log"|head -1|grep -q ' data=1 '||fail "active-high board: the saved level was not used after a restart"
echo "PASS: GPS enable level - active low at once, active high found after 20 s silence, saved and used at the next boot"
