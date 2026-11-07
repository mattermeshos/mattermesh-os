#!/usr/bin/env bash
# ThinkNode M9 Wi-Fi by keys: scan, down to the first network row, open it with OK, type a password holding characters the T-Deck keyboard has
# no key for (& = %, sent by the M9 controller as final characters), join. They were dropped, so such a password failed.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/"
cat > "$d/w.emu" <<'S'
4000 serial wifi on
4500 serial section 4
5000 serial wifi scan
9200 roll 0 1
9500 tap 21
10500 type mesh
11200 tap 338
11500 type co
12000 tap 361
12300 type re
12800 tap 337
13300 tap 21
21000 serial wifi status
21500 quit
S
timeout 40 artifacts/emu-m9 --headless --data "$d" --air off --wifi 'EMU-WiFi:mesh&co=re%' --script "$d/w.emu" </dev/null > "$d/o.log" 2>&1||true
grep WIFI_STATUS "$d/o.log"|tail -1
grep WIFI_STATUS "$d/o.log"|tail -1|grep -q 'connected=1'||{ echo "FAIL: a password with & = % does not join";exit 1; }
echo "PASS: M9 Wi-Fi by keys - password with & = % typed and joined"
