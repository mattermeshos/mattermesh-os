#!/usr/bin/env bash
# Emulator OTA boot test: the "try" state left by an install decides, at the next boot, between "installed" and
# "rolled back". A Free -> Full switch keeps the build number, so the saved edition must match the running one; a
# build is refused only after two rollbacks (one can be a power cut in the first 30 s).
set -euo pipefail
cd "$(dirname "$0")/../.."
code=$(sed -n 's/^#define MM_VERSION_CODE \([0-9]*\).*/\1/p' src/ui/build_info.h)
edition=$(sed -n 's/^static const char \*Edition="\([a-z]*\)";/\1/p' src/services/ota.cpp);other=$([ "$edition" = full ] && echo free || echo full)
run(){ # $1: mmota keys "key=int" or "key:string", $2: ms before the status (default 12000)
 data=$(mktemp -d);cp -r emu/profiles/configured/sd emu/profiles/configured/nvs.json "$data/"
 python3 - "$data/nvs.json" "$1" <<'PY'
import sys,struct
p,spec=sys.argv[1],sys.argv[2];lines=open(p).read().rstrip("\n").split("\n")
if 'N "mmota"' not in lines:lines.append('N "mmota"')
for item in spec.split():
    if "=" in item:k,v=item.split("=");lines.append(f'V "mmota" "{k}" 4 {struct.pack("<i",int(v)).hex()}')
    else:k,v=item.split(":");lines.append(f'V "mmota" "{k}" 8 {(v.encode()+b"\0").hex()}')
open(p,"w").write("\n".join(lines)+"\n")
PY
 at=${2:-12000};printf '%s serial ota status\n%s quit\n' $at $((at+300)) > "$data/script.emu"
 timeout 90 artifacts/emu-plus --headless --data "$data" --script "$data/script.emu" < /dev/null > "$data/log.txt" 2>&1 || true
 grep -a "OTA_STATUS" "$data/log.txt";echo "NVS";grep -a '"mmota"' "$data/nvs.json";rm -rf "$data"; }
fail(){ echo "FAIL: $1";echo "$out";exit 1; }
hex(){ python3 -c "import struct,sys;print(struct.pack('<i',int(sys.argv[1])).hex())" "$1"; }
out=$(run "try=$((code+7)) from=$code")
grep -q "rolled_back=1" <<<"$out" || fail "rollback of a newer build not seen"
grep -q "\"bad\" 4 $(hex $((code+7)))" <<<"$out" && grep -q "\"badn\" 4 $(hex 1)" <<<"$out" || fail "first rollback not counted once"
grep -q '"try"' <<<"$out" && fail "try left after the rollback"
grep -q "refused=0" <<<"$out" || fail "a build refused after one rollback"
out=$(run "bad=$((code+7)) badn=1 try=$((code+7)) from=$code")
grep -q "\"badn\" 4 $(hex 2)" <<<"$out" || fail "second rollback of the same build not counted"
grep -q "refused=$((code+7))" <<<"$out" || fail "a build that rolled back twice is still offered"
out=$(run "try=$code from=$code tryed:$other")
grep -q "rolled_back=1" <<<"$out" || fail "a $other image at the same build that rolled back counted as installed"
out=$(run "try=$code from=$code tryed:$edition")
grep -q "installed=1" <<<"$out" && grep -q "rolled_back=0" <<<"$out" || fail "the installed $edition image not seen as installed"
out=$(run "bad=$code badn=1 bade:$edition" 34000)
grep -q '"badn"' <<<"$out" && fail "the rollback count of the running build is kept once it is healthy"
echo "PASS: OTA boot state ($edition): rollback counted per build (twice before it is refused), same-build edition switch checked, install seen, a healthy build drops its old rollback count"
