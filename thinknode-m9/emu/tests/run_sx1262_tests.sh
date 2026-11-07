#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
if [[ -n ${SX1262_TEST_BUILD_DIR:-} ]]; then
  build_dir=$SX1262_TEST_BUILD_DIR
  mkdir -p "$build_dir"
else
  build_dir=$(mktemp -d /tmp/mattermesh-sx1262-build-XXXXXX)
  trap 'rm -rf "$build_dir"' EXIT
fi
cd "$repo_root"
cat > "$build_dir/Makefile" <<'MAKE'
.DEFAULT_GOAL := all
RL := .pio/libdeps/thinknode-m9-free/RadioLib/src
FLAGS := -std=c++17 -g -O1 -pthread -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -MMD -MP
DEFS := -DARDUINO=10812 -DRADIOLIB_STATIC_ONLY=1 -DRADIOLIB_GODMODE=1 -DUSE_SX1262 \
 -DLORA_FREQ=869.618 -DLORA_BW=62.5 -DLORA_SF=8 -DLORA_CR=8 -DLORA_TX_POWER=22 \
 -DSX126X_DIO3_TCXO_VOLTAGE=1.8 -DSX126X_RX_BOOSTED_GAIN=1 -DSX126X_DIO2_AS_RF_SWITCH=0
INCS := -Iemu/include -Iemu -Imeshcore-base/src -I$(RL) -I.pio/libdeps/thinknode-m9-free/Crypto
# Compile the real SX126x driver and all its support code, without unrelated radio/protocol modules.
SOURCES := emu/tests/test_sx1262.cpp emu/devices/sx1262.cpp emu/air/air_client.cpp \
 $(filter-out emu/core/inflate.cpp,$(wildcard emu/core/*.cpp)) meshcore-base/src/helpers/radiolib/RadioLibWrappers.cpp \
 $(RL)/Module.cpp $(RL)/Hal.cpp $(RL)/hal/Arduino/ArduinoHal.cpp \
 $(RL)/protocols/PhysicalLayer/PhysicalLayer.cpp $(wildcard $(RL)/modules/SX126x/*.cpp) \
 $(wildcard $(RL)/utils/*.cpp)
OBJECTS := $(addprefix $(BUILD)/,$(SOURCES:.cpp=.o))
all: $(BUILD)/test_sx1262 $(BUILD)/emu-air
$(BUILD)/test_sx1262: $(OBJECTS)
	$(CXX) $(FLAGS) $^ -o $@
$(BUILD)/%.o: %.cpp emu/tests/run_sx1262_tests.sh
	@mkdir -p $(dir $@)
	# RadioLib passes nullptr to memcpy with length zero in SPItransferStream.
	# Keep the dependency unmodified, suppress only that annotation check in Module.cpp.
	$(CXX) $(FLAGS) $(if $(filter $(RL)/Module.cpp,$<),-fno-sanitize=nonnull-attribute) $(DEFS) $(INCS) -c $< -o $@
$(BUILD)/emu-air: emu/air/air_broker.cpp emu/air/air_protocol.h
	$(CXX) $(FLAGS) -Wall -Wextra -Werror $< -o $@
-include $(OBJECTS:.o=.d)
MAKE
make --no-print-directory -f "$build_dir/Makefile" -j"${SX1262_TEST_JOBS:-2}" BUILD="$build_dir" CXX="${CXX:-g++}"
# LeakSanitizer cannot inspect threads under the process tracer; ASan/UBSan stay enabled.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  "$build_dir/test_sx1262" "$build_dir/emu-air" </dev/null
