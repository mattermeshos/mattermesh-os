#!/usr/bin/env bash
# M9 Home (key-only redesign): the timeline is the default Main area; up selects its rows, OK opens the chat of the
# selected message, BACK returns to the section tiles.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/"
cat > "$d/h.emu" <<'S'
4000 tap 37
4400 tap 21
4800 type ciao dal M
5800 tap 21
6400 tap 36
7000 serial status
7300 roll 0 -1
7800 serial status
8000 roll 0 -1
8300 tap 21
8800 serial status
9200 tap 36
9500 roll 0 -1
9800 tap 41
10100 serial status
10400 quit
S
timeout 40 artifacts/emu-m9 --headless --data "$d" --air off --script "$d/h.emu" </dev/null > "$d/o.log" 2>&1||true
st(){ grep '^STATUS' "$d/o.log"|sed -n "$(($1+1))p"; } # line 1: the boot STATUS
fail(){ echo "FAIL: $1";grep '^STATUS' "$d/o.log"|grep -oE ' screen=[0-9]+'|paste -sd' ';exit 1; }
st 1|grep -q ' screen=3 '||fail "HOME did not show Home"
st 2|grep -q ' screen=3 '||fail "up left Home"
st 3|grep -q ' screen=1 '||fail "OK on a timeline row did not open its chat"
st 4|grep -q ' screen=3 '||fail "BACK on a row left Home"
echo "PASS: M9 Home - timeline default, up selects rows, OK opens the chat, BACK back to the tiles"
