#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Broad native audit suite. Real radio/USB/Wi-Fi tests are deliberately separate.
bash tools/test_ui.sh
python3 tools/check_line_comments.py src
mkdir -p artifacts/release-tests
for test in services ringtones first_repeater lock repeater_observer time_sync sd_replace battery_gauge cli_line rx_log emoji_frequent keyboard_layout sky home_data; do
  g++ -std=c++17 -O2 -Isrc "sim/test_${test}.cpp" -o "artifacts/release-tests/${test}"
  "artifacts/release-tests/${test}"
done
for test in chat_clear heard_journal channel_remap sd_history journal_tail; do
  g++ -std=c++17 -O2 -Isim/journal-stubs -Isrc "sim/test_${test}.cpp" src/mesh/journal.cpp -o "artifacts/release-tests/${test}"
  "artifacts/release-tests/${test}"
done
# Compile shared UI implementation once for the additional page/regression cases.
ui_flags=(-std=c++17 -O2 -DARDUINO=10800 -Isim/stubs -Isrc -I'hardware/lib/Adafruit GFX Library')
g++ "${ui_flags[@]}" -c src/ui/components.cpp -o artifacts/release-tests/components.o
g++ "${ui_flags[@]}" -c src/ui/artwork.cpp -o artifacts/release-tests/artwork.o
g++ "${ui_flags[@]}" -c src/ui/renderer.cpp -o artifacts/release-tests/renderer.o
g++ "${ui_flags[@]}" -c 'hardware/lib/Adafruit GFX Library/Adafruit_GFX.cpp' -o artifacts/release-tests/gfx.o
gcc -O2 -c src/qr/qrcodegen.c -o artifacts/release-tests/qrcodegen.o
for test in degrees hold_banner keyboard_pages todo_text lora_connectivity gps_settings_ui menu_status_lock chat_scopes_unlock channel_gps; do
  g++ "${ui_flags[@]}" "sim/test_${test}.cpp" artifacts/release-tests/components.o artifacts/release-tests/artwork.o artifacts/release-tests/renderer.o artifacts/release-tests/gfx.o artifacts/release-tests/qrcodegen.o -o "artifacts/release-tests/${test}" -lz
  "artifacts/release-tests/${test}"
done
# Render every preview screen: no assert or crash, and every frame has content.
g++ "${ui_flags[@]}" sim/preview.cpp src/ui/components.cpp src/ui/artwork.cpp src/ui/renderer.cpp 'hardware/lib/Adafruit GFX Library/Adafruit_GFX.cpp' artifacts/release-tests/qrcodegen.o -o artifacts/release-tests/preview-plus -lz
rm -rf artifacts/release-tests/preview-plus-shots;artifacts/release-tests/preview-plus artifacts/release-tests/preview-plus-shots >/dev/null
python3 - artifacts/release-tests/preview-plus-shots <<'PY'
import sys,glob
shots=sorted(glob.glob(sys.argv[1]+'/*.ppm'));assert len(shots)>=40,len(shots)
for p in shots:
  b=open(p,'rb').read();px=b[b.index(b'255\n')+4:];assert len(px)==320*240*3,p;assert len(set(px[i:i+3] for i in range(0,len(px),3*97)))>2,'blank '+p
print(f'PASS: Plus colour previews, {len(shots)} screens rendered')
PY
