# MatterMesh OS PC emulator — T-Deck Max

The unchanged firmware in `src/`, real MeshCore, RadioLib, GxEPD2, SensorLib, HynTouch,
Adafruit keyboard/haptic drivers and `esp_codec_dev` run on Linux. Device models sit on
the emulated SPI, I2C, GPIO and UART interfaces. No board, serial port or PlatformIO
invocation is needed.

## Build and run

Requires a C++17 compiler, make, Python 3 and SDL2 development files (`sdl2-config`).
Uses the libraries already installed in `.pio/libdeps/mattermesh-offline-it` and
`hardware/lib`; it builds the **ordinary `mattermesh` configuration**, without
`MM_OFFLINE_ITALIAN` or the experimental CPU readout.

```sh
make -f emu/Makefile -j4 artifacts/emu-max artifacts/emu-air
emu/run-max.sh                         # seeds emu/data-max/sd on first launch
emu/run-max.sh emu/data-alpha --name Alpha --port-offset 100
```

The launcher runs in the foreground; Ctrl+C or closing the window exits. It preserves
an existing SD directory and NVS. The direct executable accepts:

```sh
artifacts/emu-max --data DATA --scale 3 --air off
artifacts/emu-max --headless --data DATA --script FILE --quit-after 15000
```

Options: `--data DIR` (default `emu/data-max`), `--scale N`, `--headless`, `--no-sd`,
`--script FILE`, repeated `--shot-after MS`, `--quit-after MS`, `--wifi SSID:PASS`,
`--port-offset N`, `--name NODE`, `--air PORT|off` (default 7300), and
`--gps LAT,LON[,ALT]|off` (default 44.18795,8.24555,420).

`DATA/sd` is the card, `DATA/nvs.json` stores Preferences, and screenshots go to
`DATA/screenshot-N.ppm`. `emu/profiles/configured-max/sd` supplies a valid radio
configuration named **Emu Max**. This Max firmware already starts at Home; unlike
the Plus branch, it has no welcome wizard to bypass. Without SD, the UI still boots,
but the firmware does not start MeshCore.

## Max devices

| Device | Model and real driver |
|---|---|
| GDEQ031T10 240×320 one-bit panel, CS34/DC35/BUSY37/RST9 | UC8253 commands through GxEPD2 and `TypingPanel`. Current/previous RAM, partial windows, power/deep sleep. Physical ink changes only after refresh completes. |
| Frontlight GPIO41 | PWM affects window illumination; the e-paper remains readable with its light off. |
| TCA8418 at 0x34, IRQ15, light42 | 10-event FIFO, latched interrupt/W1C status, overflow and matrix/debounce configuration registers; real Adafruit driver. |
| CST328 at 0x1A, IRQ12 | HynTouch's CST3xx probe, chip information and contact reports in portrait coordinates. The earlier CST66xx probe fails as expected. |
| XL9555 at 0x20 | Input/output/polarity/configuration ports, touch and keyboard resets, LoRa/GPS power and antenna/amplifier/motor controls. |
| BQ27220 at 0x55 | 3.92 V, 82% SOC, 25°C, capacity/current/status readings; USB changes current and charge status. The UI applies its own battery percentage policy. |
| SY6970 at 0x6A | Charger status, battery/VBUS ADC and ordinary register reads/writes. |
| DRV2605 at 0x5A | Register storage, identity and GO/RTP control; vibration writes log `EMU_VIBRATE`. |
| ES8311 at 0x18 | Real `esp_codec_dev`/ES8311 code over register I2C; IDF I2S shim discards playback and captures silence. |
| SX1262, CS3/BUSY6/RST4/DIO1=5 | Existing RadioLib command/IRQ/airtime/CAD/RSSI/SNR model, gated by XL9555 LoRa power. |
| GPS MIA-M10Q on Serial2 | Existing 38400-baud NMEA/UBX model, powered by XL9555 pin2. About 6 s cold start; configurable fix coordinates. |
| SD / NVS | Host directory / atomic Preferences file, using the existing core. |
| Wi-Fi | Virtual AP `EMU-WiFi` / `meshcore`; real TCP sockets restricted to localhost. |
| BLE / modem / BHI260AP | Existing disconnected BLE shim; modem and gyroscope absent. |

**Display timing follows the checked-in driver:** BUSY is **active LOW**, power
transitions take 45 ms, full refresh takes 1015 ms and ordinary partial takes 650 ms.
The default typing LUT takes 480 ms (48 frames at 100 Hz), followed by the firmware's
ordinary cleanup refresh. `EMU_EPD_BEGIN`/`EMU_EPD_DONE` record the refresh lifecycle.
These are UC8253 commands, rather than SSD16xx RAM/update commands. See
[`REPORT-04-max-emulator.md`](../docs/codex/REPORT-04-max-emulator.md) for sources and limitations.

## Inputs

Letters, Enter, Backspace and Space use the physical key matrix. Left/Right Shift,
Left Alt and Left Ctrl map to Shift, Alt and Sym; digits/punctuation generate the
corresponding Sym chord. `$` has its own key. F3 is the microphone key (offline
Italian dictation is disabled). Left mouse controls touch; Tab/middle mouse controls
BOOT. The Max has no trackball, so arrows/wheel do not generate movement.
F5 switches USB/battery; F12 takes a screenshot. Serial console commands can be
entered on stdin. Script events use absolute milliseconds since process start:

```text
9000 tap 21
11000 click 14 20
13000 type hello
15000 serial status
16000 usb on
17000 shot
18000 quit
```

The complete event syntax is in `devices/script.cpp`. Scripted input uses the same
chip models as SDL input.

## Shared LoRa air and Wi-Fi

```sh
artifacts/emu-air --port 7300 --log artifacts/air.log &
emu/run-max.sh emu/data-alpha --name Alpha --port-offset 100
# In another terminal:
emu/run-max.sh emu/data-bravo --name Bravo --port-offset 200
```

`--air off` disconnects the antenna while retaining radio initialization and local
TX completion. Topology files accept `A B rssi snr [loss%]` for symmetric links,
`A > B ...` for directed links, and `default rssi snr loss` or `default none`.
Unspecified topology gives −70 dBm / +10 dB links with no loss. Overlapping equal-SF/BW
frames collide; capture requires at least 6 dB advantage.

Enable “App over Wi-Fi” in firmware after joining the virtual AP to use its companion
server at **127.0.0.1:5000 + port offset**. Server sockets bind only loopback; outgoing
clients accept numeric 127/8 addresses or `localhost`, without external DNS. HTTP/TLS
remain unavailable. The virtual AP association itself does not use host networking.

## Validation

```sh
make -f emu/Makefile -j4 all test-max-devices
emu/tests/smoke_max.sh
emu/tests/run_core_tests.sh
emu/tests/run_sx1262_tests.sh
# Explicitly omit TCP broker integration on a socket-restricted host:
SX1262_TEST_LOCAL_ONLY=1 emu/tests/run_sx1262_tests.sh
```

Smoke checks physical Home screenshots, Enter → Messages, touch Back → Home, chat
typing/cleanup, radio readiness, GPS, storage, audio playback, haptics and virtual Wi-Fi.
Set `EMU_MAX_TEST_DIR` to an empty directory to retain smoke logs/screenshots.
Core and SX1262 tests use ASan/UBSan. The default SX1262 suite deliberately fails
if localhost sockets are prohibited; the explicit local-only run reports that skip.
`EMU_LIBDEPS` selects another dependency directory for the SX1262 test runner.

## Layout and Plus compatibility

`include/` and `core/` provide Arduino/IDF/FreeRTOS on Linux. `stubs/` provides the
existing Wi-Fi/HTTP/BLE/OTA compatibility interfaces. `devices/max_devices.*` contains
Max models, while `devices/board_devices.h` selects the board API used by the shared
window, script reader and entry point. Build outputs and generated version headers
stay under `artifacts/`; the firmware and dependencies are never rewritten.

The copied `plus_devices.*`, `run-plus.sh`, configured Plus profile and Plus test
scripts remain available. `BOARD=plus` retains their device/input/window build
configuration, for use with the **Plus firmware source branch**. This worktree's
`src/main.cpp` is Max-only, so it cannot build a complete Plus firmware executable.
The shared Plus entry point, SDL window, scripts and chip model are compile-checked.
