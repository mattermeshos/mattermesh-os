#!/usr/bin/env bash
# Notification card: Alpha sends a Public message to Bravo (unlocked, on Home), the card shows over the status bar
# and a tap on it opens the chat; a chat notice closes with a tap; the update and storm cards (serial "banner demo")
# open OTA update and Weather > STORMS (no update card over OTA update); a tap under the card goes to the page.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((7400+RANDOM%500))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
for n in Alpha Bravo;do mkdir -p "$d/$n";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/$n/";printf '869.618\t62.50\t8\t8\t22\t%s\t2\n' "$n" > "$d/$n/sd/mattermesh/radio.conf";done
cat > "$d/alpha.emu" <<'S'
5000 serial radio advert
9500 tap 21
10500 tap 21
11800 type Ciao Bravo
13800 tap 21
36000 quit
S
cat > "$d/bravo.emu" <<'S'
6500 serial radio advert
15500 serial banner status
16000 click 160 30
17000 serial banner status
17200 serial banner demo notice
17500 click 160 30
17800 serial banner status
18000 serial banner demo update
18600 click 160 120
19400 serial banner status
19600 click 160 30
20400 serial banner status
21000 serial banner demo storm
21600 click 160 30
22400 serial banner status
23000 serial section 17
23400 serial banner demo update
23800 serial banner status
36000 quit
S
artifacts/emu-air --port $port --log "$d/air.log" > "$d/broker.out" 2>&1 &
sleep 0.5
timeout 50 artifacts/emu-plus --headless --data "$d/Alpha" --air $port --port-offset 1400 --script "$d/alpha.emu" < /dev/null > "$d/alpha.log" 2>&1 &
timeout 50 artifacts/emu-plus --headless --data "$d/Bravo" --air $port --port-offset 1500 --script "$d/bravo.emu" < /dev/null > "$d/bravo.log" 2>&1
wait %2 2>/dev/null||true
fail(){ echo "FAIL: $1";grep -E 'BANNER|TOUCH_TAP' "$d/bravo.log"|tail -12;exit 1; }
st=$(grep "^BANNER_STATUS" "$d/bravo.log"||true)
[ "$(echo "$st"|wc -l)" -ge 7 ]||fail "no banner status"
echo "$st"|sed -n 1p|grep -q " kind=4 "||fail "the received message shows no card"
echo "$st"|sed -n 2p|grep -q " kind=0 screen=1 section=[0-9]* tab=[0-9]* chat=0$"||fail "a tap on the message card did not open its chat"
echo "$st"|sed -n 3p|grep -q " kind=0 screen=1 "||fail "a tap on the chat notice did not close it"
echo "$st"|sed -n 4p|grep -q " kind=2 screen=1 "||fail "a tap under the update card was taken by the card"
echo "$st"|sed -n 5p|grep -q " kind=0 screen=4 section=17 "||fail "a tap on the update card did not open OTA update"
echo "$st"|sed -n 6p|grep -q " kind=0 screen=4 section=10 tab=1 "||fail "a tap on the storm card did not open Weather > STORMS"
echo "$st"|sed -n 7p|grep -q " kind=0 screen=4 section=17 "||fail "the update card shows over OTA update itself"
echo "PASS: notification card - a received message, an update and a storm alert open their page with a tap"
