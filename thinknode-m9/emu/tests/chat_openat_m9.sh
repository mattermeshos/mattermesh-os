#!/usr/bin/env bash
# Chat options > Open chats at: "unread" opens a chat at its first unread message (offset = unread - 3, the newer ones
# below), "latest" at the bottom. A second T-Deck on the shared air sends 8 messages to Public while the first waits on Home.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);air=$((7400+RANDOM%500))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
for n in R S;do mkdir -p "$d/$n";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/$n/";printf '869.618\t62.50\t8\t8\t22\t%s\t2\n' "Node$n" > "$d/$n/sd/mattermesh/radio.conf";done
{ echo "6000 tap 21";echo "7000 tap 21";t=8300;for i in 1 2 3 4 5 6 7 8;do echo "$t type Message $i";t=$((t+1500));echo "$t tap 21";t=$((t+1700));done;echo "45000 quit"; } > "$d/s.emu"
cat > "$d/r.emu" <<'S'
4000 serial chat openat unread
40000 serial ui unlock
40500 serial chat open 0
41500 serial status
42000 serial key BACK
42500 serial chat openat latest
43000 serial chat open 0
44000 serial status
45000 quit
S
artifacts/emu-air --port $air > "$d/broker.out" 2>&1 &
sleep 0.5
timeout 60 artifacts/emu-m9 --headless --data "$d/S" --air $air --port-offset 3600 --script "$d/s.emu" < /dev/null > "$d/s.log" 2>&1 &
timeout 60 artifacts/emu-m9 --headless --data "$d/R" --air $air --port-offset 3700 --script "$d/r.emu" < /dev/null > "$d/r.log" 2>&1||true
fail(){ echo "FAIL: $1";grep -E 'CHAT_OPEN' "$d/r.log"|cut -c1-200;exit 1; }
mapfile -t st < <(grep -o 'CHAT_OPEN id=0 offset=[0-9]* unread=[0-9]*' "$d/r.log")
[ "${#st[@]}" -ge 2 ]||fail "missing CHAT_OPEN lines"
[[ "${st[0]}" == *"offset=5 unread=8"* ]]||fail "unread: 8 unread should open at offset 5 (${st[0]})"
[[ "${st[1]}" == *"offset=0 "* ]]||fail "latest: should open at the bottom (${st[1]})"
echo "PASS: Open chats at - unread opens at the first unread (offset 5 of 8), latest at the bottom"
