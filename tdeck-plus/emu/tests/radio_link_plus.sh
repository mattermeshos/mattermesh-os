#!/usr/bin/env bash
# Companion link (test line): Bravo uses Alpha as its radio. Alpha runs the companion server over the virtual Wi-Fi
# (the emulator stands in for the BLE companion, emu/devices/radio_link_emu.cpp); Bravo finds it, a wrong PIN is
# refused, the right one pairs and puts Bravo's own radio to sleep. Bravo's Public message goes out through Alpha
# (Charlie hears it, Bravo never transmits); Charlie's Public message reaches Bravo's chat through Alpha.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((7400+RANDOM%500));offA=$((2600+RANDOM%300))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
for n in Alpha Bravo Charlie;do mkdir -p "$d/$n";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/$n/";printf '869.618\t62.50\t8\t8\t22\t%s\t2\n' "$n" > "$d/$n/sd/mattermesh/radio.conf";done
cat > "$d/alpha.emu" <<'S'
2500 serial wifi on
3000 serial wifi join {"ssid":"EMU-WiFi","password":"meshcore"}
6000 serial app wifi
6500 serial radio advert
40000 quit
S
cat > "$d/bravo.emu" <<'S'
7800 serial section 41
8000 serial rlink scan
9000 serial rlink pair 0 111111
10500 serial rlink status
11000 serial rlink pair 0 123456
14800 serial rlink sync yes
15000 serial rlink status
15200 serial rlink info
15300 serial section 41
15400 serial rlink page
15500 serial home
15600 serial home
16000 tap 21
17000 tap 21
18300 type Ciao via companion
20300 tap 21
33000 serial rlink last
33500 serial rlink status
34000 serial rlink off
35000 serial rlink status
40000 quit
S
cat > "$d/charlie.emu" <<'S'
7000 serial radio advert
23000 tap 21
24000 tap 21
25300 type Ciao da Charlie
27300 tap 21
36000 serial radio status
40000 quit
S
artifacts/emu-air --port $port --log "$d/air.log" > "$d/broker.out" 2>&1 &
sleep 0.5
timeout 70 artifacts/emu-plus --headless --data "$d/Alpha" --air $port --port-offset $offA --script "$d/alpha.emu" < /dev/null > "$d/alpha.log" 2>&1 &
apid=$!
for i in $(seq 1 40);do grep -q "EMU_WIFI listening" "$d/alpha.log" 2>/dev/null&&break;sleep 0.5;done
timeout 70 artifacts/emu-plus --headless --data "$d/Charlie" --air $port --port-offset $((offA+300)) --script "$d/charlie.emu" < /dev/null > "$d/charlie.log" 2>&1 &
cpid=$!
MM_EMU_LINK=127.0.0.1:$((5000+offA)) MM_EMU_LINK_NAME=MeshCore-Alpha MM_EMU_LINK_PIN=123456 timeout 70 artifacts/emu-plus --headless --data "$d/Bravo" --air $port --port-offset $((offA+150)) --script "$d/bravo.emu" < /dev/null > "$d/bravo.log" 2>&1
wait $apid $cpid 2>/dev/null||true
fail(){ echo "FAIL: $1";for f in air.log alpha.log bravo.log charlie.log;do echo "--- $f";grep -v '^\[I\]' "$d/$f"|grep -v UI_READY|tail -12;done;exit 1; }
grep -q 'RLINK_STATUS source=0 on=0 saved=0 phase=4 .*message="Pairing failed: check the PIN"' "$d/bravo.log"||fail "wrong PIN not refused"
grep -q "RLINK_STATUS source=0 on=1 saved=1 phase=3 ready=1 name=MeshCore-Alpha .*channels=[1-9].* radio_suspended=1" "$d/bravo.log"||fail "Bravo not paired / ready / radio not asleep"
grep -q "RLINK_INFO name=Alpha " "$d/bravo.log"||fail "companion info (name Alpha) not read"
! grep -q "sender=Bravo " "$d/air.log"||fail "Bravo transmitted with its own radio"
grep "sender=Alpha " "$d/air.log"|grep -q "Charlie:-70/10/ok"||fail "Charlie did not hear Alpha"
grep -q "RLINK_LAST .*sender=Charlie body=Ciao da Charlie .*in=[1-9].* sent=[1-9]" "$d/bravo.log"||fail "Charlie's message did not reach Bravo, or Bravo's send was not confirmed"
grep -q "RLINK_STATUS source=0 on=0 saved=1 phase=0 .*radio_suspended=0" "$d/bravo.log"||fail "turning the link off did not wake Bravo's radio"
python3 - "$d/air.log" <<'PY'||fail "Bravo's text not on air through Alpha"
import sys
air=open(sys.argv[1]).read().splitlines()
assert sum(1 for l in air if " sender=Alpha " in l and " length=" in l and int(l.split(" length=")[1].split()[0])>=40)>=1
PY
grep -q "RLINK_SYNC asked=1 choice=1" "$d/bravo.log"||fail "the download question was not shown"
grep -q "RLINK_PAGE saved=1 name=MeshCore-Alpha" "$d/bravo.log"||fail "the page does not show the paired companion"
echo "PASS: companion link - Bravo pairs with Alpha (wrong PIN refused), its radio sleeps, its Public message goes out through Alpha and Charlie hears it, Charlie's reply reaches Bravo's chat, off wakes the radio"
