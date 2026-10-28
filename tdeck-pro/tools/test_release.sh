#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Broad native audit suite. Real radio/USB/Wi-Fi tests are deliberately separate.
python3 tools/check_line_comments.py src
bash tools/test_ui.sh
mkdir -p artifacts/release-tests
for test in services sd_replace keyboard_layout audio_pcm ringtones vibrations dictation_policy pending_keys lock repeater_observer time_sync; do
  g++ -std=c++17 -O2 -Isrc "sim/test_${test}.cpp" -o "artifacts/release-tests/${test}"
  "artifacts/release-tests/${test}"
done
for test in chat_clear heard_journal channel_remap journal_tail; do
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
for test in degrees lock_wallpaper hold_banner todo_text lora_connectivity gps_settings_ui menu_status_lock chat_scopes_unlock channel_gps; do
  g++ "${ui_flags[@]}" "sim/test_${test}.cpp" artifacts/release-tests/components.o artifacts/release-tests/artwork.o artifacts/release-tests/renderer.o artifacts/release-tests/gfx.o artifacts/release-tests/qrcodegen.o -o "artifacts/release-tests/${test}"
  "artifacts/release-tests/${test}"
done
# Dictation engine actually shipped (asr2, MMQIT002) on the deployed packed 6-bit model, when present.
model="${MM_QN15_Q6:-artifacts/offline-it/qn15/export/qn15-it-q6.mmq}"
if [ -f "$model" ]; then
  g++ -std=c++14 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -pthread -Isrc \
    src/offline_it/asr2.cpp sim/test_asr2.cpp -o artifacts/release-tests/asr2-sanitized
  ASAN_OPTIONS=detect_leaks=1 artifacts/release-tests/asr2-sanitized "$model"
else
  echo "SKIP asr2: no model at $model"
fi
