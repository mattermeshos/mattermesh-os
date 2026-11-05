#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p artifacts
gcc -O2 -c src/qr/qrcodegen.c -o artifacts/qrcodegen.o
g++ -std=c++17 -O2 -Isrc sim/test_message_banner.cpp -o artifacts/test-message-banner
artifacts/test-message-banner
g++ -std=c++17 -O2 -DARDUINO=10800 -Isim/stubs -Isrc -I'hardware/lib/Adafruit GFX Library' sim/test_ui.cpp src/ui/components.cpp src/ui/artwork.cpp src/ui/renderer.cpp 'hardware/lib/Adafruit GFX Library/Adafruit_GFX.cpp' artifacts/qrcodegen.o -o artifacts/test-ui
artifacts/test-ui
g++ -std=c++17 -O2 -DARDUINO=10800 -Isim/stubs -Isrc -I'hardware/lib/Adafruit GFX Library' sim/test_contacts_ui.cpp src/ui/components.cpp src/ui/artwork.cpp src/ui/renderer.cpp 'hardware/lib/Adafruit GFX Library/Adafruit_GFX.cpp' artifacts/qrcodegen.o -o artifacts/test-contacts-ui
g++ -std=c++17 -O2 -DARDUINO=10800 -Isim/stubs -Isrc -I'hardware/lib/Adafruit GFX Library' sim/test_flag_text.cpp src/ui/components.cpp 'hardware/lib/Adafruit GFX Library/Adafruit_GFX.cpp' -o artifacts/test-flag-text
artifacts/test-flag-text
artifacts/test-contacts-ui
g++ -std=c++17 -O2 -DARDUINO=10800 -Isim/stubs -Isrc -I'hardware/lib/Adafruit GFX Library' sim/test_status_icons.cpp src/ui/components.cpp 'hardware/lib/Adafruit GFX Library/Adafruit_GFX.cpp' -o artifacts/test-status-icons
artifacts/test-status-icons
g++ -std=c++17 -O2 -Isrc sim/test_touch_pipeline.cpp -o artifacts/test-touch-pipeline
artifacts/test-touch-pipeline
g++ -std=c++17 -O2 -Isrc sim/test_map_math.cpp -o artifacts/test-map-math
artifacts/test-map-math
g++ -std=c++17 -O2 -Isrc sim/test_panel_blit.cpp -o artifacts/test-panel-blit
artifacts/test-panel-blit
g++ -std=c++17 -O2 -Isrc sim/test_refresh_policy.cpp -o artifacts/test-refresh-policy
artifacts/test-refresh-policy
g++ -std=c++17 -O2 -Isrc sim/test_flood_policy.cpp -o artifacts/test-flood-policy
artifacts/test-flood-policy
g++ -std=c++17 -O2 -Isrc sim/test_unlock_recovery.cpp -o artifacts/test-unlock-recovery
artifacts/test-unlock-recovery
g++ -std=c++17 -O2 -Isrc sim/test_input_queue.cpp -o artifacts/test-input-queue
artifacts/test-input-queue
g++ -std=c++17 -O2 -Isrc sim/test_unread_messages.cpp -o artifacts/test-unread-messages
artifacts/test-unread-messages
g++ -std=c++17 -O2 -Isim/journal-stubs -Isrc sim/test_quick_reply.cpp src/mesh/journal.cpp -o artifacts/test-quick-reply
artifacts/test-quick-reply
g++ -std=c++17 -O2 -Isrc sim/test_message_limits.cpp -o artifacts/test-message-limits
artifacts/test-message-limits
g++ -std=c++17 -O2 -Isim/journal-stubs -Isrc sim/test_chat_session.cpp src/mesh/journal.cpp -o artifacts/test-chat-session
artifacts/test-chat-session
g++ -std=c++17 -O2 -Isim/journal-stubs -Isrc sim/test_connectivity_snapshot.cpp -o artifacts/test-connectivity-snapshot
artifacts/test-connectivity-snapshot
g++ -std=c++17 -O2 -Isrc sim/test_keyboard_input.cpp -o artifacts/test-keyboard-input
artifacts/test-keyboard-input

g++ -std=c++17 -O2 -DARDUINO=10800 -Isim/stubs -Isrc -I'hardware/lib/Adafruit GFX Library' sim/test_master_phase1.cpp src/ui/components.cpp src/ui/artwork.cpp src/ui/renderer.cpp 'hardware/lib/Adafruit GFX Library/Adafruit_GFX.cpp' artifacts/qrcodegen.o -o artifacts/test-master-phase1
artifacts/test-master-phase1

g++ -std=c++17 -O2 -Isim/time-stubs -Isim/location-stubs -Isrc sim/test_time_acquisition.cpp src/services/time_sync.cpp -Wl,--wrap=settimeofday -o artifacts/test-time-acquisition
artifacts/test-time-acquisition

g++ -std=c++17 -O2 -Isim/location-stubs -Isrc sim/test_location_power.cpp src/services/location.cpp -o artifacts/test-location-power
artifacts/test-location-power
g++ -std=c++17 -O2 -Isim/time-stubs -Isim/location-stubs -Isrc sim/test_time_sources.cpp src/services/time_sync.cpp -Wl,--wrap=settimeofday -o artifacts/test-time-sources
artifacts/test-time-sources
g++ -std=c++17 -O2 -DARDUINO=10800 -Isim/stubs -Isrc -I'hardware/lib/Adafruit GFX Library' sim/test_wifi.cpp src/ui/components.cpp src/ui/artwork.cpp src/ui/renderer.cpp 'hardware/lib/Adafruit GFX Library/Adafruit_GFX.cpp' artifacts/qrcodegen.o -o artifacts/test-wifi
artifacts/test-wifi
g++ -std=c++17 -O2 -DESP32 -Isim/time-stubs -Isim/location-stubs -Isrc sim/test_time_acquisition.cpp src/services/time_sync.cpp -Wl,--wrap=settimeofday -o artifacts/test-time-sntp-guard
artifacts/test-time-sntp-guard

g++ -std=c++17 -O2 -Isrc sim/test_chat_history.cpp -o artifacts/test-chat-history
artifacts/test-chat-history
g++ -std=c++17 -O2 -Isrc sim/test_gps_nmea.cpp -o artifacts/test-gps-nmea
artifacts/test-gps-nmea
g++ -std=c++17 -O2 -Isrc sim/test_gps_probe.cpp -o artifacts/test-gps-probe
artifacts/test-gps-probe
g++ -std=c++17 -O2 -Isrc sim/test_gps_backup.cpp -o artifacts/test-gps-backup
artifacts/test-gps-backup
g++ -std=c++17 -O2 -DARDUINO=10800 -Isim/gps-parser-stubs -Ihardware/lib/TinyGPSPlus/src -Isim/location-stubs -Isrc sim/test_gps_assist.cpp src/services/location.cpp hardware/lib/TinyGPSPlus/src/TinyGPS++.cpp -o artifacts/test-gps-assist
artifacts/test-gps-assist

g++ -std=c++17 -O2 -DARDUINO=10800 -Isim/gps-parser-stubs -Ihardware/lib/TinyGPSPlus/src -Isim/time-stubs -Isim/location-stubs -Isrc sim/test_gps_parser_time.cpp src/services/location.cpp src/services/time_sync.cpp hardware/lib/TinyGPSPlus/src/TinyGPS++.cpp -Wl,--wrap=settimeofday -o artifacts/test-gps-parser-time
artifacts/test-gps-parser-time
g++ -std=c++17 -O2 -Isrc sim/test_channel_form.cpp -o artifacts/test-channel-form
artifacts/test-channel-form
g++ -std=c++17 -O2 -Isrc sim/test_channel_qr.cpp artifacts/qrcodegen.o -o artifacts/test-channel-qr
artifacts/test-channel-qr
g++ -std=c++17 -O2 -Isrc sim/test_direct_chat.cpp -o artifacts/test-direct-chat
artifacts/test-direct-chat
