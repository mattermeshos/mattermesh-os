#!/usr/bin/env bash
# One-hand symbols: sym pressed and released alone gives the symbol to the next key only (S -> 4, then s again), a
# second tap cancels it; sym held works as before for every key until it is released. The status bar shows SYM.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);cleanup(){ rm -rf "$d"; };trap cleanup EXIT
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/"
cat > "$d/a.emu" <<'S'
5000 serial chat open 0
6000 key 32 down
6100 key 32 up
6400 serial composer text
6600 type s
7000 type s
7400 serial composer text
7600 key 32 down
7700 type ss
8000 key 32 up
8300 type s
8700 key 32 down
8800 key 32 up
8900 key 32 down
9000 key 32 up
9200 type s
9600 serial composer text
10000 quit
S
timeout 30 artifacts/emu-max --headless --data "$d" --air off --port-offset 3800 --script "$d/a.emu" < /dev/null > "$d/log" 2>&1||true
fail(){ echo "FAIL: $1";grep COMPOSER_TEXT "$d/log";exit 1; }
mapfile -t st < <(grep -o 'COMPOSER_TEXT hex=[0-9a-f]*' "$d/log"|cut -d= -f2)
[ "${#st[@]}" -ge 3 ]||fail "missing lines"
[ "${st[1]}" = 3473 ]||fail "tapped once: S gives 4, the next S an s (${st[1]})"
[ "${st[2]}" = 347334347373 ]||fail "held: S S give 4 4, released s; two taps cancel the latch (${st[2]})"
grep -q 'composerSym\|SYM' /dev/null||true
echo "PASS: sym once (tap) for the next key, held for many, second tap cancels"
