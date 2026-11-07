#!/usr/bin/env bash
# MatterMesh Web on the M9: the device joins the virtual Wi-Fi, serves the page on port 80 (+ offset), a browser logs in with
# the code, reads chats / messages / network, sends to Public, adds a #hashtag channel, logs out.
set -eu
cd "$(dirname "$0")/../.."
d=$(mktemp -d);offset=$((2000+RANDOM%500));port=$((80+offset));base="http://127.0.0.1:$port"
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/"
cat > "$d/script.emu" <<'S'
3000 serial wifi on
3500 serial wifi join {"ssid":"EMU-WiFi","password":"meshcore"}
7000 serial web on
7300 serial web code
7600 serial section 47
2500 serial web status
9000 shot
9100 serial section 19
9800 shot
60000 quit
S
timeout 70 artifacts/emu-m9 --headless --data "$d" --air off --port-offset $offset --script "$d/script.emu" < /dev/null > "$d/log.txt" 2>&1 &
for i in $(seq 1 120);do grep -q "WEB_STATUS on=1 address=http.* code=[0-9]" "$d/log.txt" 2>/dev/null&&break;sleep 0.5;done
code=$(grep -o "WEB_STATUS on=1 address=[^ ]* code=[0-9]\{4\}" "$d/log.txt"|head -1|sed "s/.*code=//")
[ -n "$code" ]||{ echo "FAIL: web not on";grep -v '^\[I\]' "$d/log.txt"|tail;exit 1; }
fail(){ echo "FAIL: $*";exit 1; }
curl -sf "$base/" > "$d/page.html" || fail "page";grep -q "MatterMesh Web" "$d/page.html" || fail "page content"
[ "$(curl -s -o /dev/null -w '%{http_code}' "$base/api/state?t=x")" = 401 ] || fail "state without login"
[ "$(curl -s -o /dev/null -w '%{http_code}' -d '{"pin":"abcd"}' "$base/api/login")" = 401 ] || fail "wrong code accepted"
tok=$(curl -sf -d "{\"pin\":\"$code\"}" "$base/api/login" | python3 -c 'import sys,json;print(json.load(sys.stdin)["token"])') || fail "login"
st=$(curl -sf "$base/api/state?t=$tok") || fail "state"
echo "$st" | python3 -c 'import sys,json;j=json.load(sys.stdin);assert j["chats"][0]["name"]=="Public",j;print("chats",len(j["chats"]))'
curl -sf "$base/api/chat?id=0&t=$tok" | python3 -c 'import sys,json;json.load(sys.stdin)["messages"]' || fail "chat"
curl -sf -d '{"chat":0,"text":"hello from the web"}' "$base/api/send?t=$tok" | grep -q '"ok":true' || fail "send"
curl -sf "$base/api/chat?id=0&t=$tok" | grep -q "hello from the web" || fail "sent message missing"
curl -sf "$base/api/chat?id=0&t=$tok" > "$d/chat.json";python3 -c 'import sys,json;m=[x for x in json.load(open(sys.argv[1]))["messages"] if x["mine"]][-1];assert m["meta"].startswith("Heard x") or m["meta"].startswith("Not sent"),m["meta"];assert "repeaters" in m' "$d/chat.json" || fail "own message meta"
for i in $(seq 1 30);do r=$(curl -s -d '{"form":3,"name":"webtest","scope":""}' "$base/api/channel?t=$tok");echo "$r"|grep -q busy||break;sleep 1;done;echo "$r"|grep -q '"id"' || fail "hashtag: $r"
curl -sf "$base/api/state?t=$tok" | grep -q '"#webtest"' || fail "new channel missing"
r=$(curl -s -d '{"form":3,"name":"webtest"}' "$base/api/channel?t=$tok");echo "$r"|grep -q 'already exists' || fail "duplicate: $r"
k=$(curl -sf "$base/api/key?t=$tok"|python3 -c 'import sys,json;print(json.load(sys.stdin)["key"])');[ ${#k} = 32 ] || fail "key"
r=$(curl -s -d "{\"form\":1,\"name\":\"Web Team\",\"key\":\"$k\"}" "$base/api/channel?t=$tok");echo "$r"|grep -q '"id"' || fail "private: $r"
curl -sf "$base/api/network?t=$tok" | python3 -c 'import sys,json;j=json.load(sys.stdin);assert "radio" in j and "nodes" in j;print("network ok, freq",j["radio"]["freq"])' || fail "network"
curl -sf -d '{}' "$base/api/logout?t=$tok" >/dev/null || fail "logout"
[ "$(curl -s -o /dev/null -w '%{http_code}' "$base/api/state?t=$tok")" = 401 ] || fail "token still valid after logout"
for i in $(seq 1 40);do [ -f "$d/screenshot-2.ppm" ]&&break;sleep 0.5;done;[ -f "$d/screenshot-2.ppm" ] || fail "no screenshots"
grep -q "CHANNEL_SAVED" "$d/log.txt" || fail "no CHANNEL_SAVED"
grep -q "WEB_STATUS on=0" "$d/log.txt" || fail "web must be off at boot"
[ -n "${KEEP_DIR:-}" ] && cp "$d"/screenshot-*.ppm "$KEEP_DIR/" || true
echo "PASS mmweb_m9"
