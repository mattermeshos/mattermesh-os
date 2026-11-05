# Native Linux build of the unchanged firmware. Default board: T-Deck Max.
# make -f emu/Makefile -j4 artifacts/emu-max
# BOARD=plus retains the copied Plus device/window configuration for the Plus firmware branch.
BOARD   ?= max
OUT     := artifacts/emu-$(BOARD)
OBJ     := artifacts/emu-obj-$(BOARD)
LIBDEPS ?= .pio/libdeps/mattermesh-free
HW      := hardware/lib
MC      := meshcore-base
# Space-free links to the libraries whose folder names contain spaces (make cannot handle them in targets).
LL      := artifacts/emu-libs
$(shell mkdir -p $(LL) && ln -sfn "$(CURDIR)/$(HW)/Adafruit GFX Library" $(LL)/gfx && ln -sfn "$(CURDIR)/$(HW)/Adafruit BusIO" $(LL)/busio \
  && ln -sfn "$(CURDIR)/$(HW)/Adafruit TCA8418" $(LL)/tca8418 \
  && ln -sfn "$(CURDIR)/$(HW)/Adafruit_DRV2605" $(LL)/drv2605)

DEFS := -DARDUINO=10812 -DMM_EMULATOR=1 -DARDUINO_USB_CDC_ON_BOOT=1 -DARDUINO_USB_MODE=1 -DBOARD_HAS_PSRAM \
 -DRADIOLIB_STATIC_ONLY=1 -DRADIOLIB_GODMODE=1 -DUSE_SX1262 -DLORA_SF=8 -DLORA_FREQ=869.618 -DLORA_BW=62.5 -DLORA_CR=8 \
 -DLORA_TX_POWER=22 -DMAX_GROUP_CHANNELS=32 -DMAX_CONTACTS=1024

ifeq ($(BOARD),plus)
DEFS += -DMM_BOARD_PLUS=1 -DMM_WIDTH=320 -DMM_HEIGHT=240 -DMM_COLOR=1 -DMM_HAPTIC=0 -DMM_DICTATION=0 -DMM_DEV_CPU_CLOCK=1
else ifeq ($(BOARD),max)
DEFS += -DMM_BOARD_MAX=1 -DMM_WIDTH=240 -DMM_HEIGHT=320 -DNOBYFOUR
RADIO_EXCLUDES := CC1101 NRF24 RF69 SX1231 SI443X RFM2X SX127X STM32WLX SX128X LR11X0 \
 AFSK AX25 HELLSCHREIBER MORSE RTTY SSTV BELL APRS LORAWAN FSK4 PAGER
DEFS += $(addprefix -DRADIOLIB_EXCLUDE_,$(addsuffix =1,$(RADIO_EXCLUDES)))
else
$(error BOARD must be max or plus)
endif

ARDUINO_LIBS := $(HOME)/.platformio/packages/framework-arduinoespressif32/libraries
INCS := -I$(OBJ)/generated -I$(OBJ)/generated/mesh -I$(OBJ)/generated/ui -Iemu/stubs -Iemu/include -Iemu -Isrc -I$(MC)/src -I$(MC)/lib/ed25519 \
 -I"$(LIBDEPS)/Crypto" -I"$(LIBDEPS)/base64/src" -I"$(LIBDEPS)/ArduinoJson/src" -I"$(LIBDEPS)/CayenneLPP/src" \
 -I"$(LIBDEPS)/PNGdec/src" -I"$(LIBDEPS)/RadioLib/src" -I$(LL)/gfx -I$(LL)/busio -I$(LL)/tca8418 -I"$(HW)/SensorLib/src" -I"$(HW)/TinyGPSPlus/src" -I"$(HW)/TDeckMaxBoard/src" -I"$(HW)/GxEPD2/src" -I"$(HW)/HynTouch/src" -I$(LL)/drv2605 -I"$(HW)/XPowersLib/src" \
 -I"$(HW)/esp_codec_dev" -I"$(HW)/esp_codec_dev/include" -I"$(HW)/esp_codec_dev/interface" -I"$(HW)/esp_codec_dev/device/include" -I"$(HW)/esp_codec_dev/device/priv_include" $(shell sdl2-config --cflags) -I$(ARDUINO_LIBS)/HTTPClient/src

CXXFLAGS := -std=gnu++17 -O1 -g -w -fno-strict-aliasing -MMD -MP $(DEFS) $(INCS) $(EMU_CXXFLAGS)
CFLAGS   := -O1 -g -w -MMD -MP $(DEFS) $(INCS) $(EMU_CFLAGS)
LDLIBS   := $(shell sdl2-config --libs) -lssl -lcrypto -lpthread $(EMU_LDFLAGS) -Wl,--wrap=free

# Match env:mattermesh: the host-only QuartzNet v1 engine is excluded.
FW_SRC  := $(shell find src \( -name '*.cpp' -o -name '*.c' \) -not -path 'src/offline_it/quartznet.cpp')
MC_SRC  := $(wildcard $(MC)/src/*.cpp) $(addprefix $(MC)/src/helpers/,BaseChatMesh.cpp AdvertDataHelpers.cpp TxtDataHelpers.cpp \
           StaticPoolPacketManager.cpp TransportKeyStore.cpp radiolib/RadioLibWrappers.cpp)
ED_SRC  := $(wildcard $(MC)/lib/ed25519/*.c)
LIB_SRC := $(wildcard $(LIBDEPS)/Crypto/*.cpp) $(shell find "$(LIBDEPS)/RadioLib/src" -name '*.cpp') \
           $(LIBDEPS)/CayenneLPP/src/CayenneLPP.cpp $(LIBDEPS)/PNGdec/src/PNGdec.cpp \
           $(addprefix $(LIBDEPS)/PNGdec/src/,adler32.c crc32.c infback.c inffast.c inflate.c inftrees.c zutil.c) \
           $(LL)/gfx/Adafruit_GFX.cpp $(LL)/busio/Adafruit_I2CDevice.cpp \
           $(LL)/busio/Adafruit_SPIDevice.cpp $(LL)/busio/Adafruit_BusIO_Register.cpp $(LL)/tca8418/Adafruit_TCA8418.cpp $(HW)/SensorLib/src/TouchDrvInterface.cpp \
           $(HW)/SensorLib/src/SensorWireHelper.cpp $(HW)/TinyGPSPlus/src/TinyGPS++.cpp \
           $(ARDUINO_LIBS)/HTTPClient/src/HTTPClient.cpp
ifeq ($(BOARD),max)
LIB_SRC += $(HW)/SensorLib/src/touch/TouchClassCST816.cpp $(HW)/SensorLib/src/touch/TouchClassCST226.cpp $(HW)/GxEPD2/src/GxEPD2_EPD.cpp $(HW)/GxEPD2/src/gdeq/GxEPD2_310_GDEQ031T10.cpp \
 $(LL)/drv2605/Adafruit_DRV2605.cpp $(wildcard $(HW)/HynTouch/src/*.cpp) $(wildcard $(HW)/HynTouch/src/*.c) \
 $(HW)/esp_codec_dev/esp_codec.cpp $(wildcard $(HW)/esp_codec_dev/esp_codec_dev*.c) $(HW)/esp_codec_dev/audio_codec_sw_vol.c \
 $(HW)/esp_codec_dev/platform/audio_codec_ctrl_i2c_arduino.cpp $(HW)/esp_codec_dev/platform/audio_codec_gpio.c \
 $(HW)/esp_codec_dev/platform/audio_codec_data_i2s.c $(HW)/esp_codec_dev/platform/esp_codec_dev_os.c \
 $(HW)/esp_codec_dev/device/es8311/es8311.c $(HW)/esp_codec_dev/device/es8388/es8388.c
endif
EMU_SRC := $(wildcard emu/core/*.cpp) $(filter-out emu/devices/max_devices.cpp emu/devices/plus_devices.cpp,$(wildcard emu/devices/*.cpp)) emu/devices/$(BOARD)_devices.cpp emu/air/air_client.cpp emu/main_emu.cpp

obj = $(OBJ)/$(basename $(1)).o
SRCS := $(FW_SRC) $(MC_SRC) $(ED_SRC) $(LIB_SRC) $(EMU_SRC)
OBJS := $(foreach s,$(SRCS),$(call obj,$(s)))

.DEFAULT_GOAL := all
all: $(OUT) artifacts/emu-air

artifacts/emu-air: emu/air/air_broker.cpp emu/air/air_protocol.h
	@mkdir -p artifacts
	$(CXX) -std=c++17 -O2 -g -Wall -Wextra -pthread emu/air/air_broker.cpp -o $@

$(OUT): $(OBJS)
	@echo "LINK $@"
	@$(CXX) -o $@ $(OBJS) $(LDLIBS)

define compile_rule
$(call obj,$(1)): $(1) emu/Makefile $(OBJ)/generated/ui/build_info.h
	@mkdir -p $$(dir $$@)
	@echo "CC $(1)"
	@$(if $(filter %.c,$(1)),$(CC) $(CFLAGS),$(CXX) $(CXXFLAGS)) -c $(1) -o $$@
endef
$(foreach s,$(SRCS),$(eval $(call compile_rule,$(s))))

# Generate version headers in the build directory: never modify firmware sources.
$(OBJ)/generated/ui/build_info.h: firmware_version.json tools/ui_build_info.py emu/build_info.py
	@python3 emu/build_info.py $(BOARD) $(OBJ)/generated


clean:
	rm -rf $(OBJ) $(OUT) artifacts/emu-air

-include $(OBJS:.o=.d) $(OBJ)/emu/tests/test_max_devices.d
.PHONY: all clean

# Device contracts exercised through the real GxEPD2, TCA8418, HynTouch and ES8311 drivers.
DEVICE_TEST_SRCS := emu/tests/test_max_devices.cpp $(LIB_SRC) $(filter-out emu/main_emu.cpp,$(EMU_SRC))
DEVICE_TEST_OBJS := $(foreach s,$(DEVICE_TEST_SRCS),$(call obj,$(s)))
$(eval $(call compile_rule,emu/tests/test_max_devices.cpp))
artifacts/test-max-devices: $(DEVICE_TEST_OBJS)
	@$(CXX) -o $@ $^ $(LDLIBS)
.PHONY: test-max-devices
test-max-devices: artifacts/test-max-devices
	@artifacts/test-max-devices </dev/null
