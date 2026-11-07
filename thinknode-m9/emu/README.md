# MatterMesh OS PC emulator (T-Deck Plus)

The firmware in `src/` compiled for Linux, unchanged, on emulated T-Deck Plus chips:

| Device | Emulated as | Firmware driver that talks to it |
|---|---|---|
| ST7789 LCD 320x240 | SPI chip (CASET/RASET/RAMWR, MADCTL, sleep/display on) shown in an SDL window | `src/board/m9_display.h` |
| Backlight | GPIO42 pulse dimming, 16 levels | `plus::backlight` |
| ESP32-C3 keyboard | I2C 0x55, raw matrix mode (5 column bytes) | `src/board/m9_keyboard.h` |
| GT911 touch | I2C 0x5D registers (id, status, points) | SensorLib `TouchDrvGT911` |
| Trackball | GPIO 3/2/15/1 line changes, BOOT GPIO0 press | `plus::Trackball` |
| Battery | ADC GPIO4 through the 1:2 divider | `plus::batteryMillivolts` |
| SD card | folder `DATA/sd` | Arduino `SD` / `FS` (emu/include) |
| NVS | `DATA/nvs.json` | `Preferences` |
| SX1262 | SPI chip model (Codex, `emu/devices/sx1262.cpp`): RadioLib command set, IRQ/DIO1, real LoRa airtime, CAD, RSSI/SNR | RadioLib + MeshCore wrapper |
| LoRa air | `artifacts/emu-air` broker: topology, RSSI/SNR, loss, collisions + capture | — |
| Wi-Fi | virtual AP `EMU-WiFi` / `meshcore`, real TCP sockets (companion server on 5000 + offset) | `emu/devices/wifi_emu.cpp` |
| HTTPS | the real Arduino-ESP32 `HTTPClient` on `WiFiClientSecure` over OpenSSL: certificates checked only against the CA the firmware passes (`setCACert`), host name + SNI | `emu/devices/tls_emu.cpp` |
| GPS MIA-M10Q | Serial2 at 38400: NMEA RMC/GGA/GSA/GSV each second, ~6 s cold start then a 3D fix at `--gps LAT,LON[,ALT]` (default Breuil-Cervinia), UBX MON-VER / ACK / backup sleep | `src/services/location.cpp` |
| BLE | never connected (use the app over Wi-Fi) | `emu/stubs` |

## Build and run
```
systemd-run --user --scope -q -p MemoryMax=4G make -f emu/Makefile -j6
artifacts/emu-plus [--data emu/data-plus] [--scale 3] [--headless] [--no-sd] [--script FILE] [--shot-after MS] [--quit-after MS]
```
A new data folder boots like a new T-Deck (welcome wizard). `emu/profiles/configured` is a configured device (SD + NVS with mmsetup/done=1; since 0.3.1.9 an empty NVS
means a clean install, which archives the SD and opens the welcome): `cp -r emu/profiles/configured/{sd,nvs.json} DATA/`
to start on Home.

## Several T-Decks on one air (phase 2)
```
artifacts/emu-air [--port 7300] [--topology FILE] [--log air.log] &
artifacts/emu-plus --data emu/data-alpha --name Alpha --port-offset 100 &
artifacts/emu-plus --data emu/data-bravo --name Bravo --port-offset 200 &
```
Every emulator joins the broker on port 7300 by default (`--air PORT`, `--air off` = no antenna). Topology lines:
`A B rssi snr [loss%]` (symmetric), `A > B ...` (one way), `default rssi snr loss` or `default none`; without a file
everyone hears everyone at -70 dBm / +10 dB. `--log` writes one line per frame (receivers, outcome, hex).
MeshCore app over Wi-Fi: on the emulator join `EMU-WiFi` (password `meshcore`, or `--wifi SSID:PASS`), enable
"App over Wi-Fi"; the server listens on 5000 + port offset at the PC's LAN address.

Headless tests: `--script FILE` plays timed inputs through the emulated chips (format at the top of
`emu/devices/script.cpp`: key, tap, type, touch, click, roll, press, serial, usb, shot, quit).
Tests: `emu/tests/run_core_tests.sh` (core), `emu/tests/run_sx1262_tests.sh` (SX1262 + air, real RadioLib/MeshCore),
`emu/tests/smoke_plus.sh` (boot, Home, keyboard, touch), `emu/tests/mesh_plus.sh` (two T-Decks: adverts + a typed
Public message), `emu/tests/companion_plus.sh` (Wi-Fi join + companion protocol over TCP), `emu/tests/keyboard_plus.sh` (keyboard languages, accent taps, Alt + Space), `emu/tests/save_fail_plus.sh` (settings that cannot be saved stay as they were; `MM_EMU_FAIL_KEY=<namespace>/<key>` makes that NVS key unwritable), `emu/tests/ota_rollback_plus.sh` (OTA boot state: rollback counted per build, Free -> Full edition check).
The serial console is the terminal: every firmware serial command works (type it and press Enter).

## Keys
letters, Enter, Backspace, Space as on the T-Deck; Left/Right Shift = Shift; Left Alt = Alt; Left Ctrl = Sym;
digits and punctuation are sent as Sym + the key that carries them; `$` is its own key; F3 = microphone key.
Arrows / mouse wheel = trackball, Tab or middle click = trackball press, left mouse = touch.
F5 = battery / USB power, F12 = screenshot (`DATA/screenshot-N.ppm`).

## Layout
- `emu/include` — Arduino-ESP32 / ESP-IDF / FreeRTOS core for Linux (Codex, TASK-02), tests in `emu/tests`
- `emu/stubs` — Wi-Fi, HTTP, BLE, OTA, mbedtls shims
- `emu/devices` — chip models and window; `emu/main_emu.cpp` — entry point; `emu/include/emu_hal.h` — the contract

## ThinkNode M9 branches
m9-free and m9-full carry the M9 only: Plus hardware code is deleted, not disabled. `make -f emu/Makefile -j4` builds `artifacts/emu-m9`; firmware envs `thinknode-m9-free` (m9-free) and `thinknode-m9` (m9-full).
Keys: arrows = D-pad, Tab = OK, Esc = Home, F3 = MIC, F6 = Time, F7 = GPS, F8 = FM, F9 = Preset, F10 = FM held. Tests: emu/tests/keys_m9.sh, func_m9.sh, mesh_m9.sh (needs PLUS_EMU), apps_m9.sh (Full), monkey_m9.py.
Nothing here has run on a real M9. To confirm on hardware: keyboard register and function-row codes (printed as M9_KEY unknown=), GPS enable polarity and chip, backlight polarity, panel orientation (serial `lcd madctl <hex>`), LR1110 RF path and TX power, battery curve.
