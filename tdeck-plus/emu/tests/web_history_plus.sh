#!/usr/bin/env bash
# MatterMesh Web history and read state: 70 received messages of Public on the SD journal (the T-Deck keeps only the
# last 16 in RAM) are all reachable from the browser, 30 newest then 40 older; a page that shows the chat (seen=1)
# marks it read on the T-Deck: a second T-Deck on the shared air sends to Public, the first counts it unread and the
# page that shows the chat (seen=1) clears it.
set -eu
cd "$(dirname "$0")/../.."
d=$(mktemp -d);offset=$((2000+RANDOM%500));port=$((80+offset));base="http://127.0.0.1:$port";air=$((7400+RANDOM%500))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/";mkdir -p "$d/S";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/S/"
printf '869.618\t62.50\t8\t8\t22\tWeb\t2\n' > "$d/sd/mattermesh/radio.conf";printf '869.618\t62.50\t8\t8\t22\tSender\t2\n' > "$d/S/sd/mattermesh/radio.conf"
python3 - "$d/sd/mattermesh/messages.tsv" <<'P'
import sys
rows=[f"R2\t{i:x}\t0\t0\t{1793900000+i*60}\t1\tNode{i%5}\tit\tReceived\tHistory message {i}\tflood:a1\t-1\t" for i in range(1,71)]
open(sys.argv[1],'w').write('\n'.join(rows)+'\n')
P
cat > "$d/script.emu" <<'S'
3000 serial wifi on
3500 serial wifi join {"ssid":"EMU-WiFi","password":"meshcore"}
7000 serial web on
7300 serial web code
60000 quit
S
cat > "$d/sender.emu" <<'S'
24000 tap 21
25000 tap 21
26300 type Fresh one
28300 tap 21
40000 quit
S
artifacts/emu-air --port $air > "$d/broker.out" 2>&1 &
sleep 0.5
timeout 50 artifacts/emu-plus --headless --data "$d/S" --air $air --port-offset $((offset+700)) --script "$d/sender.emu" < /dev/null > "$d/sender.log" 2>&1 &
timeout 70 artifacts/emu-plus --headless --data "$d" --air $air --port-offset $offset --script "$d/script.emu" < /dev/null > "$d/log.txt" 2>&1 &
for i in $(seq 1 120);do grep -q "WEB_STATUS on=1 address=http.* code=[0-9]" "$d/log.txt" 2>/dev/null&&break;sleep 0.5;done
code=$(grep -o "WEB_STATUS on=1 address=[^ ]* code=[0-9]\{4\}" "$d/log.txt"|head -1|sed "s/.*code=//")
[ -n "$code" ]||{ echo "FAIL: web not on";exit 1; }
fail(){ echo "FAIL: $*";exit 1; }
tok=$(curl -sf -d "{\"pin\":\"$code\"}" "$base/api/login" | python3 -c 'import sys,json;print(json.load(sys.stdin)["token"])') || fail "login"
unread(){ curl -sf "$base/api/state?t=$tok" | python3 -c 'import sys,json;print(json.load(sys.stdin)["chats"][0].get("unread",-1))'; }
for i in $(seq 1 60);do [ "$(unread)" -ge 1 ]&&break;sleep 1;done
u0=$(unread);[ "$u0" -ge 1 ] || fail "the message of the other T-Deck should be unread ($u0)"
curl -sf "$base/api/chat?id=0&limit=30&t=$tok" > "$d/p1.json" || fail "chat page"
python3 - "$d/p1.json" <<'P' || fail "newest page"
import sys,json;j=json.load(open(sys.argv[1]));b=[m["body"] for m in j["messages"]]
assert len(b)==30 and b[0]=="History message 42" and b[-2]=="History message 70" and b[-1]=="Fresh one" and j["more"],(len(b),b[:1],b[-2:],j["more"])
P
old=$(python3 -c 'import sys,json;print(json.load(open(sys.argv[1]))["messages"][0]["token"])' "$d/p1.json")
curl -sf "$base/api/chat?id=0&before=$old&limit=40&t=$tok" > "$d/p2.json" || fail "older page"
python3 - "$d/p2.json" <<'P' || fail "older page content"
import sys,json;j=json.load(open(sys.argv[1]));b=[m["body"] for m in j["messages"]]
assert len(b)==40 and b[0]=="History message 2" and b[-1]=="History message 41" and j["more"],(len(b),b[:1],b[-1:],j["more"])
P
[ "$(unread)" = "$u0" ] || fail "reading without seen=1 must not mark it read"
curl -sf "$base/api/chat?id=0&limit=30&seen=1&t=$tok" > /dev/null || fail "seen"
[ "$(unread)" = 0 ] || fail "seen=1 should mark the chat read on the T-Deck ($(unread))"
# Resend: an own message of Public sent from the browser, then sent again with Resend (the same body twice).
curl -sf -d '{"chat":0,"text":"resend me"}' "$base/api/send?t=$tok" > /dev/null || fail "send"
sleep 2;rt=$(curl -sf "$base/api/chat?id=0&limit=5&t=$tok" | python3 -c 'import sys,json;print([m for m in json.load(sys.stdin)["messages"] if m["mine"]][-1]["token"])')
curl -sf -d "{\"chat\":0,\"token\":$rt}" "$base/api/resend?t=$tok" > /dev/null || fail "resend"
sleep 2;n=$(curl -sf "$base/api/chat?id=0&limit=6&t=$tok" | python3 -c 'import sys,json;print(sum(1 for m in json.load(sys.stdin)["messages"] if m["mine"] and m["body"]=="resend me"))')
[ "$n" = 2 ] || fail "resend should send the same message again ($n)"
[ "$(curl -s -o /dev/null -w '%{http_code}' -d '{"chat":0,"token":5}' "$base/api/resend?t=$tok")" = 400 ] || fail "resend of a received message must be refused"
echo "PASS web_history_plus: 71 messages paged 30 + 40 from the SD, read in the browser = read on the T-Deck (was $u0 unread), Resend"
