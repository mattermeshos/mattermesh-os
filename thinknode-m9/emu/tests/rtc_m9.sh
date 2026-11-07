#!/usr/bin/env bash
# ThinkNode M9 PCF8563: a fresh RTC (VL set) is not used at boot; a time set by hand is written to it and reads back.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/"
cat > "$d/s.emu" <<'S'
8000 serial rtc status
8500 serial time set 2026-11-02 20:30
9500 serial rtc status
12000 serial rtc status
12500 quit
S
timeout 40 artifacts/emu-m9 --headless --data "$d" --air off --script "$d/s.emu" </dev/null > "$d/o.log" 2>&1||true
grep -E 'RTC_' "$d/o.log" > "$d/seen"||true;cat "$d/seen"
fail(){ echo "FAIL: $1";exit 1; }
grep -q 'RTC_BOOT valid=0' "$d/seen"||fail "a stopped RTC was trusted at boot"
sed -n 2p "$d/seen"|grep -q 'RTC_STATUS ok=0'||fail "a stopped RTC read as valid"
grep -q 'RTC_STORE ok=1' "$d/seen"||fail "the time set by hand was not written to the RTC"
e1=$(grep 'RTC_STATUS ok=1' "$d/seen"|head -1|sed -E 's/.*epoch=([0-9]+).*/\1/');e2=$(grep 'RTC_STATUS ok=1' "$d/seen"|tail -1|sed -E 's/.*epoch=([0-9]+).*/\1/')
[ -n "$e1" ]&&[ "$e2" -ge $((e1+2)) ]||fail "the RTC does not keep counting ($e1 -> $e2)"
want=$(TZ=CET-1CEST,M3.5.0,M10.5.0/3 date -d '2026-11-02 20:30' +%s);[ $((e1-want)) -ge 0 ]&&[ $((e1-want)) -le 3 ]||fail "RTC holds $e1, wanted UTC $want (20:30 Rome)"
echo "PASS: M9 RTC - stopped RTC ignored, time set by hand stored as UTC, RTC keeps counting"
