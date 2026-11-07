#!/usr/bin/env bash
# MatterMesh Web across a restart (an OTA update too): switched on, a browser logs in; after a restart the T-Deck turns
# it on by itself with the same code and the same session (the open page carries on); switched off it stays off.
set -eu
cd "$(dirname "$0")/../.."
d=$(mktemp -d);offset=$((2000+RANDOM%400));base="http://127.0.0.1:$((80+offset))"
cleanup(){ kill $(jobs -p) 2>/dev/null||true;[ -n "${KEEP:-}" ] && cp "$d"/log* "$KEEP"/ 2>/dev/null;rm -rf "$d"; };trap cleanup EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/"
fail(){ echo "FAIL: $*";exit 1; }
run(){ timeout 40 artifacts/emu-plus --headless --data "$d" --air off --port-offset $offset --script "$d/$1" < /dev/null > "$d/$2" 2>&1 & }
printf '3000 serial wifi on\n3500 serial wifi join {"ssid":"EMU-WiFi","password":"meshcore"}\n7000 serial web on\n7500 serial web code\n30000 quit\n' > "$d/a.emu"
run a.emu log1
for i in $(seq 1 60);do grep -q 'WEB_STATUS on=1.* code=[0-9]' "$d/log1" 2>/dev/null&&break;sleep 0.5;done;sleep 0.5
code=$(grep -o 'WEB_STATUS on=1.* code=[0-9]\{4\}' "$d/log1"|tail -1|grep -o '[0-9]\{4\}$')
tok=$(curl -s -m 5 -d "{\"pin\":\"$code\"}" "$base/api/login" | python3 -c 'import sys,json;print(json.load(sys.stdin).get("token",""))')
[ -n "$tok" ]||fail "login on the first run"
wait
printf '3000 serial wifi on\n9000 serial web status\n9500 serial web code\n30000 quit\n' > "$d/b.emu"
run b.emu log2
for i in $(seq 1 60);do grep -q 'WEB_STATUS.* code=[0-9]' "$d/log2" 2>/dev/null&&break;sleep 0.5;done
grep -q 'WEB_STATUS on=1' "$d/log2"||fail "web should come back on after a restart"
[ "$(grep -o 'WEB_STATUS on=1.* code=[0-9]\{4\}' "$d/log2"|tail -1|grep -o '[0-9]\{4\}$')" = "$code" ]||fail "same code after a restart"
[ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' "$base/api/state?t=$tok")" = 200 ]||fail "the session should survive the restart"
wait
printf '3000 serial web off\n4000 quit\n' > "$d/c.emu";run c.emu log3;wait
printf '3000 serial wifi on\n9000 serial web status\n12000 quit\n' > "$d/e.emu";run e.emu log4;wait
grep -q 'WEB_STATUS on=0' "$d/log4"||fail "switched off it must stay off after a restart"
echo "PASS web_restore_plus: on with the same code and session after a restart, off stays off"
