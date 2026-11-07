# MatterMesh OS PC emulator (T-Deck Plus). From the repo root:
#   systemd-run --user --scope -q -p MemoryMax=4G make -f emu/Makefile -j6
# Output: artifacts/emu-plus. Objects in artifacts/emu-obj (incremental, header dependencies tracked).
# The firmware sources, MeshCore and the libraries are the ones the tdeck-plus env builds; emu/include and
# emu/stubs shadow the Arduino-ESP32 core, ESP-IDF and the radio-less hardware libraries.
OUT     := artifacts/emu-m9
OBJ     := artifacts/emu-obj
LIBDEPS := .pio/libdeps/thinknode-m9-free
HW      := hardware/lib
MC      := meshcore-base
# Space-free links to the libraries whose folder names contain spaces (make cannot handle them in targets).
LL      := artifacts/emu-libs
$(shell mkdir -p $(LL) && ln -sfn "$(CURDIR)/$(HW)/Adafruit GFX Library" $(LL)/gfx && ln -sfn "$(CURDIR)/$(HW)/Adafruit BusIO" $(LL)/busio \
  && ln -sfn "$(CURDIR)/$(HW)/Adafruit TCA8418" $(LL)/tca8418)

DEFS := -DARDUINO=10812 -DMM_EMULATOR=1 -DPNG_MAX_BUFFERED_PIXELS=16434 -DNO_SIMD -DARDUINO_USB_CDC_ON_BOOT=1 -DARDUINO_USB_MODE=1 -DBOARD_HAS_PSRAM \
 -DRADIOLIB_STATIC_ONLY=1 -DRADIOLIB_GODMODE=1 -DUSE_LR1110 -DLORA_SF=8 -DLORA_FREQ=869.618 -DLORA_BW=62.5 -DLORA_CR=8 \
 -DLORA_TX_POWER=22 -DMAX_GROUP_CHANNELS=32 -DMAX_CONTACTS=1024

ARDUINO_LIBS := $(HOME)/.platformio/packages/framework-arduinoespressif32/libraries
INCS := -Iemu/stubs -Iemu/include -Iemu -Isrc -I$(MC)/src -I$(MC)/lib/ed25519 \
 -I"$(LIBDEPS)/Crypto" -I"$(LIBDEPS)/base64/src" -I"$(LIBDEPS)/ArduinoJson/src" -I"$(LIBDEPS)/CayenneLPP/src" \
 -I"$(LIBDEPS)/PNGdec/src" -I"$(LIBDEPS)/JPEGDEC/src" -I"$(LIBDEPS)/RadioLib/src" -I$(LL)/gfx -I$(LL)/busio -I$(LL)/tca8418 -I"$(HW)/SensorLib/src" -I"$(HW)/TinyGPSPlus/src" -I$(ARDUINO_LIBS)/HTTPClient/src $(shell sdl2-config --cflags)

CXXFLAGS := -std=gnu++17 -O1 -g -w -fno-strict-aliasing -MMD -MP $(DEFS) $(INCS) $(EMU_CXXFLAGS)
CFLAGS   := -O1 -g -w -MMD -MP $(DEFS) $(INCS) $(EMU_CFLAGS)
LDLIBS   := $(shell sdl2-config --libs) -lssl -lcrypto -lpthread $(EMU_LDFLAGS) -Wl,--wrap=free

# Firmware: what env:tdeck-plus compiles.
FW_SRC  := $(shell find src \( -name '*.cpp' -o -name '*.c' \))
MC_SRC  := $(wildcard $(MC)/src/*.cpp) $(addprefix $(MC)/src/helpers/,BaseChatMesh.cpp AdvertDataHelpers.cpp TxtDataHelpers.cpp \
           StaticPoolPacketManager.cpp TransportKeyStore.cpp radiolib/RadioLibWrappers.cpp)
ED_SRC  := $(wildcard $(MC)/lib/ed25519/*.c)
LIB_SRC := $(wildcard $(LIBDEPS)/Crypto/*.cpp) $(shell find "$(LIBDEPS)/RadioLib/src" -name '*.cpp') \
           $(LIBDEPS)/CayenneLPP/src/CayenneLPP.cpp $(LIBDEPS)/PNGdec/src/PNGdec.cpp $(LIBDEPS)/JPEGDEC/src/JPEGDEC.cpp \
           $(addprefix $(LIBDEPS)/PNGdec/src/,adler32.c crc32.c infback.c inffast.c inflate.c inftrees.c zutil.c) \
           $(LL)/gfx/Adafruit_GFX.cpp $(LL)/busio/Adafruit_I2CDevice.cpp \
           $(LL)/busio/Adafruit_SPIDevice.cpp $(LL)/busio/Adafruit_BusIO_Register.cpp $(LL)/tca8418/Adafruit_TCA8418.cpp $(HW)/SensorLib/src/TouchDrvInterface.cpp \
           $(HW)/SensorLib/src/SensorWireHelper.cpp $(HW)/TinyGPSPlus/src/TinyGPS++.cpp \
           $(ARDUINO_LIBS)/HTTPClient/src/HTTPClient.cpp
EMU_SRC := $(wildcard emu/core/*.cpp) $(wildcard emu/devices/*.cpp) emu/air/air_client.cpp emu/main_emu.cpp

obj = $(OBJ)/$(basename $(1)).o
SRCS := $(FW_SRC) $(MC_SRC) $(ED_SRC) $(LIB_SRC) $(EMU_SRC)
OBJS := $(foreach s,$(SRCS),$(call obj,$(s)))

all: $(OUT) artifacts/emu-air

artifacts/emu-air: emu/air/air_broker.cpp emu/air/air_protocol.h
	@mkdir -p artifacts
	$(CXX) -std=c++17 -O2 -g -Wall -Wextra -pthread emu/air/air_broker.cpp -o $@

$(OUT): $(OBJS)
	@echo "LINK $@"
	@$(CXX) -o $@ $(OBJS) $(LDLIBS)

define compile_rule
$(call obj,$(1)): $(1) src/ui/build_info.h
	@mkdir -p $$(dir $$@)
	@echo "CC $(1)"
	@$(if $(filter %.c,$(1)),$(CC) $(CFLAGS),$(CXX) $(CXXFLAGS)) -c $(1) -o $$@
endef
$(foreach s,$(SRCS),$(eval $(call compile_rule,$(s))))

# Version headers, as tools/ui_build_info.py writes them for pio (env tdeck-plus).
src/ui/build_info.h: firmware_version_m9.json
	@python3 emu/build_info.py

clean:
	rm -rf $(OBJ) $(OUT) artifacts/emu-air

-include $(OBJS:.o=.d)
.PHONY: all clean
