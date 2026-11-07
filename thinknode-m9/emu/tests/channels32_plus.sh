#!/usr/bin/env bash
# Emulator test: 32 conversations (MAX_GROUP_CHANNELS) imported from the app survive a reboot: slots 24..31 ready,
# a message on the last one crosses the air, a cleared chat in that slot stays cleared.
set -euo pipefail
cd "$(dirname "$0")/../.."
timeout 600 python3 emu/tools/channels32_reboot.py "$PWD"
