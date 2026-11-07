#!/usr/bin/env bash
# Start (or restart) the T-Deck Plus emulator window on a data folder, detached from the terminal.
#   emu/run-plus.sh [DATA_DIR] [extra emu-plus options...]     (default DATA_DIR emu/data-plus)
# An instance already running on the same folder is closed first (matched by process name + --data, never by
# a pattern that could hit the calling shell). A new folder starts from the configured profile (Home).
set -euo pipefail
cd "$(dirname "$0")/.."
data=${1:-emu/data-plus};[ $# -gt 0 ]&&shift
for pid in $(ps -eo pid=,comm= | awk '$2=="emu-plus"{print $1}');do
 if tr '\0' ' ' < /proc/$pid/cmdline | grep -q -- "--data $data "; then kill -TERM "$pid";for i in 1 2 3 4 5 6;do kill -0 "$pid" 2>/dev/null||break;sleep 0.5;done;kill -9 "$pid" 2>/dev/null||true;fi
done
if [ ! -d "$data/sd" ];then mkdir -p "$data";cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$data/";fi
setsid nohup artifacts/emu-plus --data "$data" --scale 3 "$@" < /dev/null > "$data/console.log" 2>&1 &
echo "emu-plus started on $data (console: $data/console.log)"
