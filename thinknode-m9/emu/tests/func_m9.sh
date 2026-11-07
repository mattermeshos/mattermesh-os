#!/usr/bin/env bash
# ThinkNode M9 functions: first start with keys only, NMEA GPS at 115200 and its enable line, buzzer, battery held while Wi-Fi owns ADC2.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);cleanup(){ rm -rf "$d"; };trap cleanup EXIT
fail(){ echo "FAIL: $1";grep -vE '^\[I\]' "$d/$2"|tail -12|cut -c1-170;exit 1; }
# First start: an empty NVS and SD, the welcome walked with OK and a typed name.
mkdir -p "$d/new"
cat > "$d/new.emu" <<'S'
3000 tap 21
3800 roll 0 1
4200 tap 21
5000 tap 21
5800 tap 21
6600 type Kilo
8400 tap 21
9200 tap 21
10000 tap 21
10800 tap 21
12000 serial status
12500 quit
S
timeout 30 artifacts/emu-m9 --headless --data "$d/new" --air off --script "$d/new.emu" </dev/null > "$d/new.log" 2>&1
grep -q 'WELCOME_DONE' "$d/new.log"||fail "the welcome cannot be finished with keys" new.log
grep -q 'WELCOME_DONE.*name=[^ ]*Kilo' "$d/new.log"||fail "typed name not used" new.log
# Configured device.
mkdir -p "$d/c";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/c/"
cat > "$d/c.emu" <<'S'
9000 serial gps status
9500 serial gps off
10000 serial gps status
11500 serial gps status
12000 serial gps on
21000 serial gps status
21500 serial audio test
21700 serial battery raw
22000 serial wifi on
24000 serial wifi status
24500 serial battery raw
36000 serial ringtone status
36200 serial battery status
37000 quit
S
timeout 45 artifacts/emu-m9 --headless --data "$d/c" --air off --script "$d/c.emu" </dev/null > "$d/c.log" 2>&1
grep 'GPS_STATUS' "$d/c.log"|sed -n 1p|grep -q 'enabled=1'||fail "GPS not on at boot" c.log
grep -E 'GPS_STATUS' "$d/c.log"|sed -n 1p|grep -qE 'fix=1'||fail "no fix from the NMEA module" c.log
grep -E 'GPS_STATUS' "$d/c.log"|sed -n 3p|grep -qE 'bytes=0|enabled=0'||fail "GPS still talking after off" c.log
grep -E 'GPS_STATUS' "$d/c.log"|sed -n 4p|grep -qE 'fix=1'||fail "no fix after GPS on" c.log
grep -q 'BATTERY_STATUS mv=[1-9].*percent=[1-9]' "$d/c.log"||fail "battery percent lost while Wi-Fi holds ADC2" c.log
grep -q 'EMU_BUZZER' "$d/c.log"||fail "no buzzer" c.log
grep 'GPS_STATUS' "$d/c.log"|cut -c1-120|sed -n 1p
grep -E 'BATTERY|WIFI_STATUS' "$d/c.log"|cut -c1-120
echo "PASS: M9 functions - welcome by keys, GPS on/off with fix, battery"
