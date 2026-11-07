#!/usr/bin/env bash
# Emulator OTA under bmorcelli's Launcher: with its "test" app partition present (MM_EMU_LAUNCHER=1, see
# emu/stubs/esp_partition.h) the firmware reports launcher=1, so updates go to the SD card; without it nothing changes
# (launcher=0, the normal OTA of the two-slot layout).
set -euo pipefail
cd "$(dirname "$0")/../.."
run(){ # $1: MM_EMU_LAUNCHER value ("" = unset)
 data=$(mktemp -d);cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$data/"
 printf '12000 serial ota status\n12300 quit\n' > "$data/script.emu"
 if [ -n "$1" ];then MM_EMU_LAUNCHER=$1 timeout 90 artifacts/emu-plus --headless --data "$data" --script "$data/script.emu" < /dev/null > "$data/log.txt" 2>&1 || true
 else env -u MM_EMU_LAUNCHER timeout 90 artifacts/emu-plus --headless --data "$data" --script "$data/script.emu" < /dev/null > "$data/log.txt" 2>&1 || true;fi
 grep -a "OTA_BOOT\|OTA_STATUS" "$data/log.txt";rm -rf "$data"; }
fail(){ echo "FAIL: $1";echo "$out";exit 1; }
out=$(run 1)
grep -q "OTA_BOOT .*launcher=1" <<<"$out" || fail "Launcher partition not seen at boot"
grep -q "OTA_STATUS .*launcher=1 to_sd=0 sd_file=" <<<"$out" || fail "ota status does not report the Launcher"
out=$(run "")
grep -q "OTA_BOOT .*launcher=0" <<<"$out" || fail "a normal installation reported as started by the Launcher"
grep -q "OTA_STATUS .*launcher=0 to_sd=0 sd_file=" <<<"$out" || fail "ota status of a normal installation changed"
grep -q "phase=7\|phase=8" <<<"$out" && fail "a normal installation left the idle OTA phase"
echo "PASS: OTA under the Launcher: its partition is seen (launcher=1, updates to the SD card); a normal installation is unchanged (launcher=0)"
