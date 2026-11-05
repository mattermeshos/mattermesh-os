#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
build_dir=$(mktemp -d /tmp/mattermesh-core-build-XXXXXX)
trap 'rm -rf "$build_dir"' EXIT
cd "$repo_root"
"${CXX:-g++}" -std=c++17 -g -O1 -pthread -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
  -Iemu/include emu/tests/test_core.cpp $(ls emu/core/*.cpp | grep -v /inflate.cpp) -o "$build_dir/test_core"
# emu/core/inflate.cpp is left out: it needs PNGdec's zlib internals (zutil.h) and only the UI (ui/packed.h) uses it.
# LeakSanitizer cannot inspect threads under this environment's process tracer.
# ASan and UBSan remain enabled and abort immediately on errors.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  "$build_dir/test_core" </dev/null
