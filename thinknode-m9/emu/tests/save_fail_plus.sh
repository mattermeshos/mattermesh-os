#!/usr/bin/env bash
# Emulator save-failure test: with one NVS key made unwritable (MM_EMU_FAIL_KEY), a setting that cannot be saved
# stays as it was on screen and in NVS: chat text size ("chatlg") and keyboard languages ("lang" fails after "mask"
# was written, so the old mask must be written back). MM_EMU_FAIL_KEY takes namespace/key.
set -euo pipefail
cd "$(dirname "$0")/../.."
run(){ # $1 failing key, $2 script
 data=$(mktemp -d);cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$data/";printf '%s\n' "$2" > "$data/script.emu"
 MM_EMU_FAIL_KEY=$1 timeout 60 artifacts/emu-plus --headless --data "$data" --script "$data/script.emu" < /dev/null > "$data/log.txt" 2>&1
 cat "$data/log.txt";echo "NVS_DUMP";cat "$data/nvs.json";rm -rf "$data"; }
fail(){ echo "FAIL: $1";exit 1; }
out=$(run mmui/chatlg "12500 serial textsize set 0
12800 quit")
grep -aq "TEXTSIZE_SET ok=0 large=1" <<<"$out" || fail "text size changed although it was not saved"
out=$(run mmkbd/lang "12500 serial section 12
13300 type s
13600 tap 21
14400 serial keyboard languages
14700 quit")
grep -aq "KEYBOARD_LANGUAGES mask=1 current=IT" <<<"$out" || fail "keyboard language turned on although it was not saved"
grep -aq '"mmkbd" "mask" 5 01000000' <<<"$out" || fail "the old keyboard mask was not written back"
echo "PASS: settings that cannot be saved stay as they were (text size, keyboard languages with the mask written back)"
