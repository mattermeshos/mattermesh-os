#!/usr/bin/env bash
# 433 MHz T-Deck with an SX1268 (MM_EMU_SX1268: the emulated chip reports "SX1268"): the radio starts, two such units
# hear each other on 433.650 MHz, an 868 frequency is refused with a clear reason, a unit still set for 868 starts on
# EU 433 Narrow at 10 dBm, and the welcome proposes 433 and finishes with the radio. A unit whose radio is not found
# (MM_EMU_NO_RADIO) still finishes the welcome: settings saved, Home, a clear notice.
# (The emulator does not model the SX1268 power table: the TX power itself is checked on hardware.)
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((7900+RANDOM%500))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
for n in Alpha Bravo Charlie Delta;do mkdir -p "$d/$n";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/$n/";done
for n in Alpha Bravo;do printf '433.650\t62.50\t8\t8\t10\t%s\t2\n' "$n" > "$d/$n/sd/mattermesh/radio.conf";done
printf '869.618\t62.50\t8\t8\t22\tCharlie\t2\n' > "$d/Charlie/sd/mattermesh/radio.conf"
cat > "$d/alpha.emu" <<'S'
5000 serial radio advert
12000 serial radio config 869.618 62.5 8 8 10
12500 serial radio status
S
cat > "$d/bravo.emu" <<'S'
6500 serial radio advert
12000 serial contacts status
16000 quit
S
# The welcome: Start, Wi-Fi skip, band, preset, name, path hash, Finish.
welcome(){ local t=$1;printf '%s serial welcome start\n' $t;printf '%s serial key ENTER\n' $((t+500));printf '%s serial key j\n' $((t+1000))
 for i in 1 2 3 4 5 6;do printf '%s serial key ENTER\n' $((t+1000+i*500));done;printf '%s serial read /mattermesh/radio.conf\n' $((t+5000));printf '%s quit\n' $((t+5500)); }
welcome 13000 >> "$d/alpha.emu"
printf '6000 serial radio status\n6200 serial read /mattermesh/radio.conf\n6500 quit\n' > "$d/charlie.emu"
welcome 4000 > "$d/delta.emu"
artifacts/emu-air --port $port --log "$d/air.log" > "$d/broker.out" 2>&1 &
sleep 0.5
MM_EMU_SX1268=1 timeout 60 artifacts/emu-plus --headless --data "$d/Alpha" --air $port --port-offset 1600 --script "$d/alpha.emu" < /dev/null > "$d/alpha.log" 2>&1 &
MM_EMU_SX1268=1 timeout 60 artifacts/emu-plus --headless --data "$d/Charlie" --air $port --port-offset 1900 --script "$d/charlie.emu" < /dev/null > "$d/charlie.log" 2>&1 &
MM_EMU_NO_RADIO=1 timeout 60 artifacts/emu-plus --headless --data "$d/Delta" --air $port --port-offset 1800 --script "$d/delta.emu" < /dev/null > "$d/delta.log" 2>&1 &
MM_EMU_SX1268=1 timeout 60 artifacts/emu-plus --headless --data "$d/Bravo" --air $port --port-offset 1700 --script "$d/bravo.emu" < /dev/null > "$d/bravo.log" 2>&1
wait %2 %3 %4 2>/dev/null||true
fail(){ echo "FAIL: $1";for f in air.log alpha.log bravo.log charlie.log delta.log;do echo "--- $f";grep -v '^\[I\]' "$d/$f"|tail -10;done;exit 1; }
for n in alpha bravo;do grep -q "RADIO_BOOT ready=1 frequency=433.650 band=433 .*power=10 error=0 chip=SX1268" "$d/$n.log"||fail "$n SX1268 not started on 433.650";done
grep -q "sender=Alpha receivers=Bravo:-70/10/ok" "$d/air.log"||fail "Bravo did not hear Alpha"
grep -q "sender=Bravo receivers=Alpha:-70/10/ok" "$d/air.log"||fail "Alpha did not hear Bravo"
grep -q "CONTACTS_STATUS .*total=1 live=1" "$d/bravo.log"||fail "Bravo has no live contact"
grep -q "RADIO_CONFIG This T-Deck radio (SX1268) is for 433 MHz" "$d/alpha.log"||fail "868 not refused with its reason"
grep -q "RADIO_STATUS .*frequency=433.650" "$d/alpha.log"||fail "an 868 frequency was applied to the SX1268"
grep -q "WELCOME_STARTED band=433" "$d/alpha.log"||fail "the welcome does not propose 433 on an SX1268"
grep -q "WELCOME_DONE preset=EU 433MHz (Narrow) .*radio=1$" "$d/alpha.log"||fail "the SX1268 welcome did not finish with the radio"
grep -q "RADIO_BOOT ready=1 frequency=433.650 band=433 bw=62.50 sf=8 cr=8 power=10 error=0 chip=SX1268" "$d/charlie.log"||fail "an SX1268 set for 868 did not start on EU 433 Narrow at 10 dBm"
grep -A1 FILE_BEGIN "$d/charlie.log" | grep -q "^869.618	62.50	8	8	22	Charlie	2"||fail "the fitted 433 setting was saved over the user's"
grep -q "RADIO_BOOT ready=0 .*error=-2 chip=none" "$d/delta.log"||fail "Delta: radio should be missing"
grep -q "WELCOME_STARTED band=868" "$d/delta.log"||fail "Delta: welcome band"
grep -q "WELCOME_DONE preset=EU/UK (Narrow) name=MatterMesh-.* radio=0 notice=Saved. Radio not found" "$d/delta.log"||fail "Delta: the welcome did not finish without a radio"
grep -q "MatterMesh-" "$d/delta.log" && grep -A1 FILE_BEGIN "$d/delta.log" | grep -q "^869.618	.*	MatterMesh-"||fail "Delta: welcome settings not saved"
echo "PASS: SX1268 (433 MHz T-Deck) - started and heard on 433.650, 868 refused, an 868 setting starts on EU 433, welcome on 433; without a radio the welcome still finishes with a notice"
