#!/usr/bin/env bash
# Emulator smoke test (phase 1): boots the T-Deck Plus emulator headless on a configured SD, checks the drivers
# found the emulated chips, opens Messages with Enter and returns Home by touch, and compares the screens.
set -euo pipefail
cd "$(dirname "$0")/../.."
data=$(mktemp -d);trap 'rm -rf "$data"' EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$data/"
cat > "$data/script.emu" <<'SCRIPT'
12500 shot
12700 tap 21
13700 shot
13900 click 14 20
14900 shot
15200 type ab
16000 quit
SCRIPT
timeout 60 artifacts/emu-plus --headless --data "$data" --script "$data/script.emu" < /dev/null > "$data/log.txt" 2>&1
fail(){ echo "FAIL: $1";tail -20 "$data/log.txt";exit 1; }
grep -q "PLUS_TOUCH ready=1" "$data/log.txt" || fail "GT911 not found by the touch driver"
grep -q "PLUS_KEYBOARD ready=1 raw=1" "$data/log.txt" || fail "C3 keyboard not in raw mode"
grep -q "BOOT_SD frequency=4000000 mounted=1" "$data/log.txt" || fail "SD not mounted"
grep -q "UI_READY screen=3" "$data/log.txt" || fail "Home not reached"
python3 - "$data" <<'PY' || fail "screens"
import sys
d=sys.argv[1]
def px(n):
    b=open(f"{d}/screenshot-{n}.ppm","rb").read();return b[b.index(b"255\n")+4:]
home,messages,back=px(1),px(2),px(3)
assert len(home)==320*240*3, "frame size"
assert len(set(home[i:i+3] for i in range(0,len(home),3*97)))>4, "Home is blank"
assert home!=messages, "Enter did not open Messages"
assert home==back or sum(a!=b for a,b in zip(home,back))<len(home)//50, "touch Back did not return Home"
PY
echo "PASS: emulator smoke - chips found (GT911, C3 raw, SD), Home, Enter -> Messages, touch Back -> Home"
