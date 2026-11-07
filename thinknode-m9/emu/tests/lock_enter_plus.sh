#!/usr/bin/env bash
# Lock screen ENTER: with the screen dark the first ENTER only lights it; on the lit screen a tap shows the next
# message and ENTER held 2 s opens the quick reply. Bravo is locked, Alpha sends two Public messages to it.
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
15500 tap 21
16500 type Secondo
18000 tap 21
42000 quit
S
cat > "$d/bravo.emu" <<'S'
3000 serial ui lock
6500 serial radio advert
27000 serial lock messages status
27500 tap 21
27800 serial lock messages status
29000 serial quick reply status
29200 tap 21
30200 serial lock messages status
30400 serial quick reply status
31000 key 21 down
31200 serial quick reply status
32000 serial quick reply status
33500 key 21 up
34000 serial quick reply status
42000 quit
S
artifacts/emu-air --port $port --log "$d/air.log" > "$d/broker.out" 2>&1 &
sleep 0.5
timeout 60 artifacts/emu-plus --headless --data "$d/Alpha" --air $port --port-offset 1400 --script "$d/alpha.emu" < /dev/null > "$d/alpha.log" 2>&1 &
timeout 60 artifacts/emu-plus --headless --data "$d/Bravo" --air $port --port-offset 1500 --script "$d/bravo.emu" < /dev/null > "$d/bravo.log" 2>&1
wait %2 2>/dev/null||true
fail(){ echo "FAIL: $1";grep -E 'QUICK_REPLY|LOCK_STATUS|UI_LOCK|RADIO_RX|incoming' "$d/bravo.log"|tail -12;exit 1; }
grep -q "UI_LOCK locked=1" "$d/bravo.log"||fail "Bravo not locked"
qr=$(grep "^QUICK_REPLY sent=" "$d/bravo.log"||true)
[ "$(echo "$qr"|wc -l)" -ge 5 ]||fail "no quick reply status"
echo "$qr"|sed -n 1p|grep -q " active=0 "||fail "the first ENTER on the dark screen opened the quick reply"
echo "$qr"|sed -n 2p|grep -q " active=0 .* dots_paints=0$"||fail "a tap on the lit screen opened the quick reply or painted the hold dots"
echo "$qr"|sed -n 3p|grep -q " active=0 .* dots=-1 dots_paints=0$"||fail "the hold dots showed before QUICK_REPLY_SHOW_MS"
echo "$qr"|sed -n 4p|grep -q " active=0 .* dots=1 dots_paints=[1-9][0-9]*$"||fail "no hold dots painted after QUICK_REPLY_SHOW_MS"
echo "$qr"|sed -n 5p|grep -q " active=1 "||fail "ENTER held 2.5 s did not open the quick reply"
lm=$(grep "^LOCK_MESSAGES" "$d/bravo.log"||true)
c1=$(echo "$lm"|sed -n 2p|grep -o 'current=[0-9]*'||true);c2=$(echo "$lm"|sed -n 3p|grep -o 'current=[0-9]*'||true)
[ -n "$c1" ]&&[ "$c1" != "$c2" ]||fail "a tap did not move to another message ($c1 -> $c2)"
echo "PASS: lock ENTER - dark: first ENTER only lights the screen; lit: a tap shows the next message with no dots, ENTER held 2 s opens the quick reply"
