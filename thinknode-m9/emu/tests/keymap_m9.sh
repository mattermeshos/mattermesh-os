#!/usr/bin/env bash
# ThinkNode M9 function keys with the codes of a real M9 (2026-11-02): MSG -> Chats, MAP -> Maps, HOME -> Home, BACK,
# CTRL -> Quick settings, triangle -> choose a message in a chat (OK: its menu), HOME unlocks a locked M9, the keyboard light goes to the
# keyboard controller (register 0x02).
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/"
cat > "$d/k.emu" <<'S'
4000 tap 37
4400 serial status
4800 tap 43
5200 serial status
5600 tap 36
6000 serial status
6400 tap 40
6800 serial status
7200 tap 41
7600 serial status
8000 tap 37
8300 tap 21
8600 type ciao
9000 tap 21
9400 tap 42
9600 tap 21
9800 serial status
10000 serial ui lock
10400 tap 36
10800 serial lock status
11000 serial kbdlight 77
11400 serial lights status
11800 quit
S
timeout 40 artifacts/emu-m9 --headless --data "$d" --air off --script "$d/k.emu" </dev/null > "$d/k.log" 2>&1||true
fail(){ echo "FAIL: $1";grep -E '^STATUS|LOCK_STATUS|LIGHTS|M9_KEY' "$d/k.log"|cut -c1-160|tail -10;exit 1; }
st(){ grep "^STATUS" "$d/k.log"|sed -n "$(($1+1))p"; } # line 1 is the boot STATUS
st 1|grep -q ' screen=0 '||fail "MSG did not open the chat list"
st 2|grep -q ' section=9 '||fail "MAP did not open Maps"
st 3|grep -q ' screen=3 .* section=0 '||fail "HOME did not go Home (from Maps)"
st 4|grep -q ' section=18 '||fail "CTRL did not open Quick settings"
st 5|grep -q ' screen=3 '||fail "BACK did not close Quick settings"
st 6|grep -q ' message_actions=1 '||fail "the triangle and OK did not open the message menu"
grep 'LOCK_STATUS' "$d/k.log"|tail -1|grep -q 'locked=0'||fail "HOME does not unlock"
grep -q 'M9_KEY unknown' "$d/k.log"&&fail "a key code went unhandled"
echo "PASS: M9 key map - MSG, MAP, HOME, CTRL, BACK, triangle, HOME unlocks"
