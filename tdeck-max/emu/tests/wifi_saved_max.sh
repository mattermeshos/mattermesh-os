#!/usr/bin/env bash
# Wi-Fi > SAVED (Max): the saved networks with FORGET (tap twice; the first tap expires after 4 s), by touch and keys,
# BACK and DONE close the page, forgetting the connected network drops it, the change survives a restart.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
cp -r emu/profiles/configured-max/sd emu/profiles/configured-max/nvs.json "$d/"
json='[{"s":"EMU-WiFi","p":"meshcore"},{"s":"Casa","p":"password1"},{"s":"Bar","p":""},{"s":"Ufficio","p":"ufficio123"},{"s":"Mare","p":"mare12345"},{"s":"Monte","p":"monte1234"},{"s":"Lago","p":"lago12345"}]'
printf 'N "mmconnect"\nV "mmconnect" "networks" 8 %s00\n' "$(printf '%s' "$json"|od -An -tx1|tr -d ' \n')" >> "$d/nvs.json"
# SAVED button (190,141); row 0 FORGET (190,97), row 1 (190,133); DONE (120,297); header BACK (14,20). Code 21 = ENTER.
cat > "$d/a.emu" <<'S'
8000 serial ui unlock
11000 serial wifi status
11300 serial section 4
11800 serial wifi saved
12000 shot
12200 click 190 141
12800 serial wifi saved
13000 shot
13400 click 190 97
13800 serial wifi saved
14000 shot
14200 click 190 97
14800 serial wifi saved
15200 serial wifi status
15600 click 190 97
20600 click 190 97
21200 serial wifi saved
21600 type j
22400 tap 21
23000 tap 21
23600 serial wifi saved
24000 shot
24400 click 14 20
25000 serial wifi saved
25400 serial status
25800 type v
26400 click 120 297
27000 serial wifi saved
27400 serial wifi forget 99
27800 quit
S
MM_EMU_LICENSED=1 timeout 90 artifacts/emu-max --headless --data "$d" --air off --script "$d/a.emu" < /dev/null > "$d/a.log" 2>&1||true
printf '6000 serial wifi saved\n6500 quit\n' > "$d/b.emu"
MM_EMU_LICENSED=1 timeout 60 artifacts/emu-max --headless --data "$d" --air off --script "$d/b.emu" < /dev/null > "$d/b.log" 2>&1||true
if [[ -n ${SHOTS:-} ]];then cp "$d"/*.ppm "$SHOTS";fi
saved=$(grep -o 'WIFI_SAVED .*' "$d/a.log"||true);echo "$saved"
fail(){ echo "FAIL: $1";grep -E 'WIFI_|TOUCH' "$d/a.log"|tail -30;exit 1; }
row(){ sed -n "${1}p" <<<"$saved"; }
grep -q 'WIFI_STATUS on=1 connected=1' "$d/a.log" || fail 'joins the saved EMU-WiFi'
[[ $(row 1) == 'WIFI_SAVED count=7 open=0 '*'names=EMU-WiFi|Casa|Bar|Ufficio|Mare|Monte|Lago' ]] || fail 'seven saved networks, page closed'
[[ $(row 2) == *'count=7 open=1 focus=0 armed=-1'* ]] || fail 'SAVED button opens the page'
[[ $(row 3) == *'count=7 open=1 focus=0 armed=0'* ]] || fail 'first FORGET tap only arms'
[[ $(row 4) == *'count=6 open=1 '*'armed=-1 names=Casa|Bar|Ufficio|Mare|Monte|Lago' ]] || fail 'second tap forgets EMU-WiFi'
grep -q 'WIFI_STATUS on=1 connected=0' "$d/a.log" || fail 'forgetting the connected network drops it'
[[ $(row 5) == *'count=6 open=1 focus=0 armed=0 '* ]] || fail 'a tap after 5 s arms again, it does not forget'
[[ $(row 6) == *'count=5 open=1 '*'names=Casa|Ufficio|Mare|Monte|Lago' ]] || fail 'j + ENTER twice forgets Bar'
[[ $(row 7) == *'count=5 open=0 '* ]] || fail 'BACK closes the saved page'
grep -q 'STATUS .*section=4' "$d/a.log" || fail 'BACK stays on the Wi-Fi page'
[[ $(row 8) == *'count=5 open=0 '* ]] || fail 'v opens, DONE closes the saved page'
grep -q 'WIFI_FORGET saved=5 armed=-1' "$d/a.log" || fail 'a missing row is ignored'
grep -q 'WIFI_SAVED count=5 open=0 .*names=Casa|Ufficio|Mare|Monte|Lago' "$d/b.log" || { tail -20 "$d/b.log"; fail 'kept after a restart'; }
echo 'PASS: Wi-Fi saved networks (Max), FORGET twice by touch and keys, expiry, BACK / DONE, connected one dropped, kept after restart'
