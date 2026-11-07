#!/usr/bin/env bash
# ThinkNode M9: everything a finger did on the T-Deck is reached with the keys alone. Emulator keys (emu/devices/m9_devices.cpp):
# 34 OK held, 36 HOME, 37 MSG, 38 ADV held, 39 ADV, 40 CTRL, 41 BACK, 42 triangle, 43 MAP, 44 @. STATUS and LOCK_STATUS lines are read back at each step, screenshots go to KEEP if set.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);cleanup(){ [ -n "${KEEP:-}" ]&&cp "$d"/*.ppm "$KEEP/" 2>/dev/null;rm -rf "$d"; };trap cleanup EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/"
cat > "$d/k.emu" <<'S'
3000 tap 40
3400 serial status
3600 shot
3800 roll 1 0
4000 roll 1 0
4200 serial speaker status
4400 roll 0 1
4600 roll 0 1
4800 roll 1 0
5000 shot
5200 tap 21
5600 serial status
5800 tap 40
6200 serial status
6400 tap 36
6800 serial status
7000 tap 37
7400 serial status
7600 tap 37
8000 shot
8200 serial status
8400 tap 41
8800 tap 36
9000 tap 21
9400 shot
9600 serial status
10000 tap 21
10400 shot
10800 tap 40
11200 shot
11400 tap 41
11800 tap 36
12000 tap 39
12400 tap 38
12800 serial status
13000 tap 34
13400 serial lock status
13600 serial ui lock
14000 serial lock status
14200 tap 34
14600 serial lock status
15000 quit
S
timeout 40 artifacts/emu-m9 --headless --data "$d" --air off --script "$d/k.emu" < /dev/null > "$d/k.log" 2>&1
fail(){ echo "FAIL: $1";grep -E 'STATUS|M9_KEY' "$d/k.log"|cut -c1-170|tail -14;exit 1; }
sec(){ grep '^STATUS' "$d/k.log"|sed -n "$1p"|grep -oE ' (screen|section)=[0-9]+'|tr -d '\n'; }
sec 2|grep -q "section=18"||fail "Preset does not open Quick settings: $(sec 2)"
grep -q 'SPEAKER_STATUS' "$d/k.log"||fail "no speaker status"
# OK held (34 here) does not lock; on a locked M9 (auto-lock, here "ui lock") it unlocks.
grep 'LOCK_STATUS' "$d/k.log"|sed -n 1p|grep -q 'locked=0'||fail "OK held locked the M9"
grep 'LOCK_STATUS' "$d/k.log"|sed -n 2p|grep -q 'locked=1'||fail "ui lock did not lock"
grep 'LOCK_STATUS' "$d/k.log"|tail -1|grep -q 'locked=0'||fail "OK held does not unlock"
grep -q 'M9_KEY unknown' "$d/k.log"&&fail "a key code went unhandled"
grep '^STATUS' "$d/k.log"|grep -oE ' (screen|section|channel_muted|message_actions)=[0-9-]+'|paste -sd' '|cut -c1-300
echo "PASS: M9 keys - Quick settings, Home, Chats, chat options, OK held unlocks"
