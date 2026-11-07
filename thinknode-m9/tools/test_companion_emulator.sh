#!/usr/bin/env bash
# Builds and runs the PC companion emulator (sim/companion_emulator.cpp) on the real MeshCore core.
set -euo pipefail
cd "$(dirname "$0")/.."
C=.pio/libdeps/thinknode-m9-free/Crypto;M=meshcore-base;O=artifacts/companion-emulator-ed25519;mkdir -p $O
for f in $M/lib/ed25519/*.c;do gcc -c -O1 -w "$f" -o $O/$(basename "$f" .c).o;done
g++ -std=c++17 -O1 -w -Isim/meshcore-host -Isrc -I$M/src -I$M/lib/ed25519 -I$C -I.pio/libdeps/thinknode-m9-free/base64/src \
 sim/companion_emulator.cpp $M/src/Mesh.cpp $M/src/Dispatcher.cpp $M/src/Packet.cpp $M/src/Identity.cpp $M/src/Utils.cpp \
 $M/src/helpers/BaseChatMesh.cpp $M/src/helpers/StaticPoolPacketManager.cpp $M/src/helpers/AdvertDataHelpers.cpp $M/src/helpers/TxtDataHelpers.cpp \
 $C/AES128.cpp $C/AESCommon.cpp $C/BlockCipher.cpp $C/Crypto.cpp $C/SHA256.cpp $C/SHA512.cpp $C/Hash.cpp $C/Ed25519.cpp $C/Curve25519.cpp $C/BigNumberUtil.cpp $C/RNG.cpp \
 $C/ChaCha.cpp $C/Cipher.cpp $O/*.o -o artifacts/companion-emulator
artifacts/companion-emulator
