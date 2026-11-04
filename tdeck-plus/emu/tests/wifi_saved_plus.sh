#!/usr/bin/env bash
# Wi-Fi > SAVED: the saved networks with FORGET (tap twice; the first tap expires after 4 s), by touch and trackball,
# BACK and DONE close the page, forgetting the connected network drops it, the change survives a restart.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$d/"
json='[{"s":"EMU-WiFi","p":"meshcore"},{"s":"Casa","p":"password1"},{"s":"Bar","p":""},{"s":"Ufficio","p":"ufficio123"},{"s":"Mare","p":"mare12345"}]'
printf 'N "mmconnect"\nV "mmconnect" "networks" 8 %s00\n' "$(printf '%s' "$json"|od -An -tx1|tr -d ' \n')" >> "$d/nvs.json"
# SAVED pill y 65; row 0 FORGET (260,94), row 1 (260,123); DONE (270,225); header BACK (14,34).
cat > "$d/a.emu" <<'S'
6000 serial ui unlock
9000 serial wifi status
9300 serial section 4
9800 serial wifi saved
10200 click 270 65
10800 serial wifi saved
11000 shot
11400 click 260 94
11800 serial wifi saved
12200 click 260 94
12800 serial wifi saved
13200 serial wifi status
13600 click 260 94
18600 click 260 94
19200 serial wifi saved
19600 roll 0 1
20400 pressclick
21000 pressclick
21600 serial wifi saved
22000 shot
22400 click 14 34
23000 serial wifi saved
23400 serial status
23800 click 270 65
24400 click 270 225
25000 serial wifi saved
25400 serial wifi forget 99
25800 quit
S
timeout 60 artifacts/emu-plus --headless --data "$d" --air off --script "$d/a.emu" < /dev/null > "$d/a.log" 2>&1||true
printf '4000 serial wifi saved\n4500 quit\n' > "$d/b.emu"
timeout 40 artifacts/emu-plus --headless --data "$d" --air off --script "$d/b.emu" < /dev/null > "$d/b.log" 2>&1||true
if [[ -n ${SHOTS:-} ]];then cp "$d"/*.ppm "$SHOTS";fi
saved=$(grep -o 'WIFI_SAVED .*' "$d/a.log"||true);echo "$saved"
fail(){ echo "FAIL: $1";grep -E 'WIFI_|TOUCH' "$d/a.log"|tail -30;exit 1; }
row(){ sed -n "${1}p" <<<"$saved"; }
grep -q 'WIFI_STATUS on=1 connected=1' "$d/a.log" || fail 'joins the saved EMU-WiFi'
[[ $(row 1) == 'WIFI_SAVED count=5 open=0 '*'names=EMU-WiFi|Casa|Bar|Ufficio|Mare' ]] || fail 'five saved networks, page closed'
[[ $(row 2) == *'count=5 open=1 focus=0 armed=-1'* ]] || fail 'SAVED pill opens the page'
[[ $(row 3) == *'count=5 open=1 focus=0 armed=0'* ]] || fail 'first FORGET tap only arms'
[[ $(row 4) == *'count=4 open=1 '*'armed=-1 names=Casa|Bar|Ufficio|Mare' ]] || fail 'second tap forgets EMU-WiFi'
grep -q 'WIFI_STATUS on=1 connected=0' "$d/a.log" || fail 'forgetting the connected network drops it'
[[ $(row 5) == *'count=4 open=1 focus=0 armed=0 '* ]] || fail 'a tap after 5 s arms again, it does not forget'
[[ $(row 6) == *'count=3 open=1 '*'names=Casa|Ufficio|Mare' ]] || fail 'trackball down + press twice forgets Bar'
[[ $(row 7) == *'count=3 open=0 '* ]] || fail 'BACK closes the saved page'
grep -q 'STATUS .*section=4' "$d/a.log" || fail 'BACK stays on the Wi-Fi page'
[[ $(row 8) == *'count=3 open=0 '* ]] || fail 'DONE closes the saved page'
grep -q 'WIFI_FORGET saved=3 armed=-1' "$d/a.log" || fail 'a missing row is ignored'
grep -q 'WIFI_SAVED count=3 open=0 .*names=Casa|Ufficio|Mare' "$d/b.log" || { tail -20 "$d/b.log"; fail 'kept after a restart'; }
echo 'PASS: Wi-Fi saved networks, FORGET twice by touch and trackball, expiry, BACK / DONE, connected one dropped, kept after restart'
