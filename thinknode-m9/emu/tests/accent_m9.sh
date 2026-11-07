#!/usr/bin/env bash
# M9 accented letters: in the chat composer Ctrl + e = è, Ctrl again = é; letters typed after it are plain; Ctrl + i = ì.
# Typed "perch" Ctrl e Ctrl "cos" Ctrl i -> "perchécosì".
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/"
printf '4000 tap 37\n4400 tap 21\n5000 type perch\n5800 tap 40\n6100 type e\n6500 tap 40\n6900 type cos\n7400 tap 40\n7700 type i\n8100 serial composer text\n8400 quit\n' > "$d/a.emu"
timeout 30 artifacts/emu-m9 --headless --data "$d" --air off --script "$d/a.emu" </dev/null > "$d/a.log" 2>&1||true
hex=$(grep -o 'COMPOSER_TEXT hex=[0-9a-f]*' "$d/a.log"|tail -1|cut -d= -f2)
[ "$hex" = "$(printf 'perchécosì'|od -An -tx1|tr -d ' \n')" ]||{ echo "FAIL: composer holds hex=$hex, not perchécosì";exit 1; }
echo "PASS: M9 accents - Ctrl + letter, Ctrl again for the next form, plain letters after it"
