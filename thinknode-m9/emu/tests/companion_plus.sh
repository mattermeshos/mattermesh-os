#!/usr/bin/env bash
# Emulator phase 2 test: the firmware joins the virtual Wi-Fi with its own connect path, starts the MeshCore
# companion server ("App over Wi-Fi") and emu/tools/companion_client.py talks to it like the phone app.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);offset=$((2000+RANDOM%500))
cleanup(){ kill $(jobs -p) 2>/dev/null||true;rm -rf "$d"; };trap cleanup EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/"
cat > "$d/script.emu" <<'S'
3000 serial wifi on
3500 serial wifi join {"ssid":"EMU-WiFi","password":"meshcore"}
7000 serial wifi status
7200 serial app wifi
30000 quit
S
timeout 60 artifacts/emu-plus --headless --data "$d" --air off --port-offset $offset --script "$d/script.emu" < /dev/null > "$d/log.txt" 2>&1 &
for i in $(seq 1 40);do grep -q "EMU_WIFI listening" "$d/log.txt" 2>/dev/null&&break;sleep 0.5;done
grep -q "WIFI_STATUS on=1 connected=1" "$d/log.txt"||{ echo "FAIL: Wi-Fi not connected";grep -v '^\[I\]' "$d/log.txt"|tail;exit 1; }
python3 emu/tools/companion_client.py 127.0.0.1 $((5000+offset))
