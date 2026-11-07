#!/usr/bin/env bash
# Emulator keyboard test: Settings > Keyboard languages turns French and Russian on, then in a chat the accent key
# is tapped before each letter (1 tap first variant, 2 taps second), Alt + Space switches language, a send clears the
# composer and leaves an armed accent unused, the exact letters are checked (composer text), Italian two taps after "ch"
# give è, five taps disarm the key, and Backspace or leaving the chat drops an armed accent.
set -euo pipefail
cd "$(dirname "$0")/../.."
data=$(mktemp -d);trap 'rm -rf "$data"' EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$data/"
cat > "$data/script.emu" <<'SCRIPT'
12500 serial section 12
13300 type sss
13600 tap 21
14000 type sss
14300 tap 21
14700 serial keyboard languages
15000 serial home
15600 tap 21
16400 tap 21
17200 tap 31
17400 type e
17700 tap 31
17900 type e
18200 serial ui state
18400 tap 33
18600 key 30 down
18700 tap 33
18900 key 30 up
19100 serial keyboard languages
19400 tap 31
19600 type p
19900 tap 31
20000 tap 31
20200 type p
20500 tap 31
20700 type l
21000 serial ui state
21200 tap 33
21400 key 30 down
21500 tap 33
21700 key 30 up
21900 serial keyboard languages
22200 type cr
22500 tap 31
22700 type e
23000 tap 31
23200 type e
23600 serial ui state
23700 serial composer text
23800 shot
23900 tap 31
24000 tap 21
24800 serial ui state
25000 serial accent status
25200 tap 31
25300 tap 31
25400 tap 31
25500 tap 31
25600 serial accent status
25800 tap 31
26000 serial accent status
26100 type x
26200 tap 31
26400 tap 11
26600 type a
26900 serial ui state
27100 tap 31
27300 click 14 20
28100 tap 21
28900 serial accent status
29100 type a
29400 serial ui state
29600 key 30 down
29700 tap 33
29900 key 30 up
30100 serial keyboard languages
30300 tap 33
30500 type perch
31500 tap 31
31700 tap 31
31900 type e
32300 serial composer text
32600 quit
SCRIPT
timeout 90 artifacts/emu-plus --headless --data "$data" --script "$data/script.emu" < /dev/null > "$data/log.txt" 2>&1
fail(){ echo "FAIL: $1";grep -a "KEYBOARD_LANGUAGES\|UI_STATE\|ACCENT_STATUS" "$data/log.txt" | cut -c1-120;exit 1; }
python3 - "$data" <<'PY' || fail "keyboard sequence"
import re,sys
d=sys.argv[1];log=open(f"{d}/log.txt",errors="replace").read()
langs=re.findall(r"KEYBOARD_LANGUAGES mask=(\d+) current=(\w+)",log)
assert [l[1] for l in langs]==["IT","RU","FR","IT"] and langs[0][0]=="73", langs  # IT + FR + RU on, Alt + Space: RU, FR, back to IT
sizes=[int(b) for b in re.findall(r"UI_STATE screen=1 .*?bytes=(\d+)",log)]
assert sizes==[4,11,18,0,1,2], sizes  # "Èè", " хъж", " créé" (2-byte letters), the send empties the composer; then plain "a" after Backspace and after reopening the chat
taps=[int(t) for t in re.findall(r"ACCENT_STATUS .*?taps=(\d+)",log)]
texts=re.findall(r"COMPOSER_TEXT hex=([0-9a-f]*)",log)
assert texts==["c388c3a820d185d18ad0b6206372c3a9c3a9","6161207065726368c3a8"], texts  # "Èè хъж créé"; "aa perchè" (two taps after ch: è)
assert taps==[0,4,0,0], taps  # nothing armed after the send; four taps armed; the fifth disarms; nothing armed after Back and reopening
b=open(f"{d}/screenshot-1.ppm","rb").read();assert len(b[b.index(b"255\n")+4:])==320*240*3, "frame size"
PY
echo "PASS: keyboard languages and accent taps (FR + RU on; Èè, хъж, créé; Alt + Space; exact letters; send clears an armed accent; 5th tap disarms; Italian 2 taps; Backspace and leaving the chat drop the accent)"
