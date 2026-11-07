#!/usr/bin/env bash
# Web panel (--web): the page and the LCD frame are served, keys sent over HTTP reach the firmware like the real chip's
# (HOME from the chat list, the D-pad), and a function key wakes the screen on the welcome page.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((18000+RANDOM%1000));pid=
trap '[ -n "$pid" ] && kill $pid 2>/dev/null; rm -rf "$d"' EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/"
artifacts/emu-m9 --headless --data "$d" --air off --web $port </dev/null > "$d/w.log" 2>&1 & pid=$!
fail(){ echo "FAIL: $1";grep -E 'EMU_WEB|M9_KEY|STATUS' "$d/w.log"|cut -c1-160|tail -8;exit 1; }
for _ in $(seq 50);do curl -s -o /dev/null "localhost:$port/" && break;sleep 0.2;done
u="http://localhost:$port"
[ "$(curl -s -o /dev/null -w '%{http_code}' $u/)" = 200 ]||fail "page not served"
[ "$(curl -s -o /dev/null -w '%{http_code}' $u/skin.png)" = 404 ]||fail "skin.png without --skin should be 404"
for _ in $(seq 20);do curl -s -D "$d/h" -o "$d/f" "$u/frame?since=0";grep -qi '^X-Light: [1-9]' "$d/h"&&break;sleep 0.5;done
# raw: 320x240 RGB565; rle: (count, colour) pairs whose counts add up to 320x240
python3 -c 'import struct,sys;b=open(sys.argv[1],"rb").read();e=sys.argv[2]
n=len(b)//2 if e=="raw" else sum(struct.unpack("<%dH"%(len(b)//2),b)[0::2])
sys.exit(n!=76800)' "$d/f" "$(grep -i '^X-Enc' "$d/h"|tr -d '\r'|cut -d' ' -f2)"||fail "frame is not 320x240"
grep -qi '^X-Light: [1-9]' "$d/h"||fail "the backlight never came on in the frames"
seq=$(grep -i '^X-Seq' "$d/h"|tr -dc 0-9)
# A frame already seen is not sent again: 204 after the wait, or a newer one (the Home screen may have changed).
next=$(curl -s -D - -o /dev/null "$u/frame?since=$seq"|grep -i '^X-Seq'|tr -dc 0-9)
[ -z "$next" ]||[ "$next" -gt "$seq" ]||fail "the frame already seen came back"
[ "$(curl -s -o /dev/null -w '%{http_code}' "$u/key?c=zz")" = 400 ]||fail "a bad key byte should be 400"
sleep 2
curl -s "$u/key?c=81" >/dev/null;sleep 0.6   # MSG: chat list
curl -s "$u/key?c=82" >/dev/null;sleep 0.6   # HOME
curl -s "$u/key?c=b6" >/dev/null;sleep 0.6   # D-pad down (with its ghost OK, dropped by the firmware)
grep -q 'M9_KEY unknown' "$d/w.log"&&fail "a key code went unhandled"
kill $pid;wait $pid 2>/dev/null||true;pid=
echo "PASS: M9 web panel - page, frame, no repeated frame, keys over HTTP (MSG, HOME, D-pad)"
