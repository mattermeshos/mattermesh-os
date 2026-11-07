#!/usr/bin/env bash
# Code swallowed by a // comment: an edit appended code right after a line comment (2026-11-03, four times: HOME,
# the N shortcut, the inputRows declaration, the RX Log row page). Looks for code shapes inside comments: "case X:",
# "else if(", "return x;", "f(...);", "){". Two comments that only name a call are known.
set -euo pipefail
cd "$(dirname "$0")/../.."
P='//[^"]*(case [A-Za-z0-9_:]+:[^ ]|else if\(|return [a-zA-Z_][^ ;]*;|[a-zA-Z_]\([^)]*\);|\)\{)'
hits=$(grep -nE "$P" src/*.cpp src/*.inc src/*/*.cpp src/*/*.h src/*/*.inc 2>/dev/null|grep -vE 'call before bleEnabled\(\); name becomes|texts from device::otaState\(\); otaProgress'||true)
if [ -n "$hits" ];then echo "$hits"|cut -c1-160;echo "FAIL: code inside a // comment";exit 1;fi
echo "PASS: no code inside // comments"
