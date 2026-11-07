#!/usr/bin/env bash
# Emulator test for the first T-Deck's keyboard: its ESP32-C3 runs the character-only firmware (MM_EMU_KB_OLD=1:
# 0x03 ignored, a 5-byte read padded with zeros). The firmware must not take it for a raw matrix: after two keys it
# switches to character mode and every letter, including the first two, reaches the composer exactly once.
set -euo pipefail
cd "$(dirname "$0")/../.."
data=$(mktemp -d);trap 'rm -rf "$data"' EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$data/"
cat > "$data/script.emu" <<'SCRIPT'
12500 serial home
13000 tap 21
13800 tap 21
14600 type hello world
17000 serial composer text
17500 quit
SCRIPT
MM_EMU_KB_OLD=1 timeout 60 artifacts/emu-plus --headless --air off --data "$data" --script "$data/script.emu" < /dev/null > "$data/log.txt" 2>&1 || true
fail(){ echo "FAIL: $1";grep -a 'PLUS_KEYBOARD\|COMPOSER_TEXT' "$data/log.txt";exit 1; }
grep -aq 'PLUS_KEYBOARD raw=0 (character firmware)' "$data/log.txt" || fail 'character firmware not detected'
want=$(printf 'hello world' | od -An -tx1 | tr -d ' \n')
grep -aq "COMPOSER_TEXT hex=$want " "$data/log.txt" || fail 'composer text is not "hello world"'
echo 'PASS: first T-Deck keyboard (character firmware): detected on the first keys, "hello world" typed exactly'
