#!/usr/bin/env bash
# Direct message routes (Chat options > Path): Alpha opens a DM with Bravo (Charlie also on the air).
# 1 first DM by flood, the path is learned (direct, 0 hops); 2 a learned path that gets no ACK (as if stale: "dm path
# learn ff") is dropped and the DM goes once more by flood and is delivered; 3 a manual path with no ACK says so and
# stays; 4 RESET: flood again; 5 Flood only keeps flooding with a learned path.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);port=$((7400+RANDOM%500))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
for n in Alpha Bravo Charlie;do mkdir -p "$d/$n";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/$n/";printf '869.618\t62.50\t8\t8\t22\t%s\t2\n' "$n" > "$d/$n/sd/mattermesh/radio.conf";done
cat > "$d/alpha.emu" <<'S'
2000 serial power display 0
3000 serial radio advert
9000 serial contact picker
10000 serial key ENTER
11000 serial type m1
11500 serial key ENTER
24000 serial dm path -1
24500 serial dm path learn -1 ffff
25000 serial type m2
25500 serial key ENTER
90000 serial dm path -1
90500 serial dm path set -1 eeee
91000 serial type m3
91500 serial key ENTER
150000 serial dm path -1
150500 serial dm path reset -1
151000 serial type m4
151500 serial key ENTER
164500 serial dm path -1
165000 serial dm flood -1 on
165500 serial type m5
166000 serial key ENTER
180000 serial dm path -1
180500 serial dm line status
182000 quit
S
cat > "$d/bravo.emu" <<'S'
5000 serial radio advert
182000 quit
S
cat > "$d/charlie.emu" <<'S'
20000 serial radio advert
182000 quit
S
artifacts/emu-air --port $port --log "$d/air.log" > "$d/broker.out" 2>&1 &
sleep 0.5
timeout 200 artifacts/emu-plus --headless --data "$d/Bravo" --air $port --port-offset 1300 --script "$d/bravo.emu" < /dev/null > "$d/bravo.log" 2>&1 &
timeout 200 artifacts/emu-plus --headless --data "$d/Charlie" --air $port --port-offset 1400 --script "$d/charlie.emu" < /dev/null > "$d/charlie.log" 2>&1 &
timeout 200 artifacts/emu-plus --headless --data "$d/Alpha" --air $port --port-offset 1200 --script "$d/alpha.emu" < /dev/null > "$d/alpha.log" 2>&1
fail(){ echo "FAIL: $1";grep -E 'TX_STATE|DM_|PICKER' "$d/alpha.log"|tail -30;exit 1; }
states=$(grep -o 'TX_STATE token=[0-9a-f]* state=.*' "$d/alpha.log")
grep -q 'state=Flood: delivered' <<<"$states"||fail "first DM not delivered by flood"
grep -q 'DM_PATH chat=[0-9]* mode=0 route="Direct (in radio range)"' "$d/alpha.log"||fail "path not learned after the first DM"
grep -q 'state=No ACK: retrying by flood' <<<"$states"||fail "stale learned path: no flood retry"
grep -q 'state=Flood (retry): delivered' <<<"$states"||fail "flood retry not delivered"
grep -q 'state=Manual path: no ACK' <<<"$states"||fail "manual path without ACK not reported"
grep -q 'DM_PATH chat=[0-9]* mode=2 route="1 hop: eeee"' "$d/alpha.log"||fail "manual path not kept"
grep -q 'DM_PATH_RESET ok=1' "$d/alpha.log"||fail "reset refused"
[ "$(grep -c 'state=Flood: delivered' <<<"$states")" -ge 3 ]||fail "after RESET / Flood only the DMs did not go by flood"
[ "$(grep -c 'DM_PATH chat=[0-9]* mode=0 route="Direct (in radio range)"' "$d/alpha.log")" -ge 3 ]||fail "the path was not learned again after RESET"
grep -q 'DM_PATH chat=[0-9]* mode=1 route="Flood"' "$d/alpha.log"||fail "Flood only not kept"
grep -q 'DM_LINE_STATUS .*line=DM path: flood only' "$d/alpha.log"||fail "DM path line does not say flood only"
echo "PASS: DM routes - learned path, stale path dropped with one flood retry, manual path kept with its No ACK, RESET and Flood only flood"
