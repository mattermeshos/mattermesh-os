#!/usr/bin/env bash
# Boot unchanged Max firmware; inputs travel through the modeled TCA8418 and CST328.
set -euo pipefail
cd "$(dirname "$0")/../.."
if [[ -n ${EMU_MAX_TEST_DIR:-} ]]; then
  data=$EMU_MAX_TEST_DIR
  mkdir -p "$data"
  [[ ! -e "$data/sd" ]] || { echo 'Use an empty EMU_MAX_TEST_DIR'; exit 2; }
else
  data=$(mktemp -d)
  trap 'rm -rf "$data"' EXIT
fi
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$data/"
cat > "$data/script.emu" <<'SCRIPT'
9000 serial status
9500 shot
10000 tap 21
12000 serial status
12500 shot
13000 click 14 20
15000 serial status
15500 shot
16000 tap 21
18000 tap 21
20000 serial status
20500 type hello
22000 serial composer status
22500 serial typing status
23000 serial ui stats
23500 serial audio test
25000 serial speaker status
25500 serial haptic test
26000 usb on
26500 serial wifi join {"ssid":"EMU-WiFi","password":"meshcore"}
28500 serial wifi link
29000 serial gps status
30000 serial status
30500 serial device status
31000 quit
SCRIPT
timeout 60 artifacts/emu-max --headless --air off --data "$data" --script "$data/script.emu" < /dev/null > "$data/console.log" 2>&1
fail(){ echo "FAIL: $1"; tail -45 "$data/console.log"; exit 1; }
grep -q 'RADIO_BOOT ready=1 .*error=0' "$data/console.log" || fail 'radio initialization'
grep -q 'BOOT_SD frequency=4000000 mounted=1' "$data/console.log" || fail 'SD initialization'
grep -q 'STATUS .*keyboard=1 .*screen=3 .*touch=1 ' "$data/console.log" || fail 'Home and real input drivers'
grep -q 'TOUCH_TAP x=14 y=20 screen=0' "$data/console.log" || fail 'CST328 touch Back'
grep -q 'COMPOSER_STATUS bytes=5 ' "$data/console.log" || fail 'typing through keyboard FIFO'
grep -q 'EMU_EPD_BEGIN mode=typing ms=480 ' "$data/console.log" || fail 'typing LUT waveform'
grep -q 'SPEAKER_STATUS ready=1 .*played=1 .*error=0' "$data/console.log" || fail 'codec playback'
grep -q 'EMU_VIBRATE ' "$data/console.log" || fail 'haptic events'
grep -q 'WIFI_LINK {"connected":true' "$data/console.log" || fail 'virtual Wi-Fi AP'
grep -q 'GPS_STATUS enabled=1 available=1 .*fix=1 .*bad=0 ' "$data/console.log" || fail 'GPS fix'
python3 - "$data" <<'PY' || fail 'screens and refresh lifecycle'
from pathlib import Path
import re, sys
root=Path(sys.argv[1]); log=(root/'console.log').read_text()
def pixels(n):
    magic,size,limit,data=(root/f'screenshot-{n}.ppm').read_bytes().split(b'\n',3)
    assert (magic,size,limit)==(b'P6',b'240 320',b'255'), 'panel dimensions'
    assert len(data)==240*320*3
    colors=set(data[i:i+3] for i in range(0,len(data),3))
    assert colors=={b'\0\0\0',b'\xff\xff\xff'}, 'one-bit physical panel'
    assert 1000<data.count(0)<len(data)//2, 'nonblank screen'
    return data
home,messages,back=map(pixels,(1,2,3))
assert home!=messages, 'Enter opens Messages'
assert sum(a!=b for a,b in zip(home,back))<len(home)*0.03, 'touch restores Home (clock may advance)'
# Driver waits must complete before firmware announces that its frame is ready.
home_ready=log.index('UI_READY screen=3')
assert log.rfind('EMU_EPD_DONE',0,home_ready)>log.rfind('EMU_EPD_BEGIN',0,home_ready)
screens=[int(s) for s in re.findall(r'^STATUS .*? screen=(\d+)',log,re.M)]
assert screens[:5]==[3,3,0,3,1], screens
stats=re.search(r'UI_STATS .*?busy=(\d+)',log)
assert stats and int(stats[1])>50, 'firmware services input/radio while panel is busy'
assert 'Busy Timeout!' not in log, 'driver BUSY timeout'
assert (root/'nvs.json').is_file(), 'persistent NVS'
print('PASS Max smoke: physical Home, Enter -> Messages, touch -> Home, typing, radio, GPS, SD/NVS, audio, haptic, virtual Wi-Fi')
PY
