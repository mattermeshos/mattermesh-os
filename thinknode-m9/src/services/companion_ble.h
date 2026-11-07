#pragma once
// Companion link, transport side: the T-Deck as a BLE central of another MeshCore companion (its Nordic UART
// service: one write = one frame out, one notify = one frame in). Connect and pairing run in a worker task and writes
// in a sender task, so the UI keeps going (the scan is asynchronous in NimBLE); frames cross to the main loop through a PSRAM ring. On the device this is NimBLE
// (companion_ble.cpp); the emulator bridges the same calls to the TCP companion port of another emulator
// (emu/devices/radio_link_emu.cpp).
#include <Arduino.h>
#include <vector>
namespace device {
struct RadioLinkPeer {String name,address;uint8_t addressType=0;int rssi=0;};
enum class RadioLinkPhase {Off,Scanning,Connecting,Connected,Failed};
struct RadioLinkStatus {RadioLinkPhase phase=RadioLinkPhase::Off;String message,name,address;uint32_t revision=0,connects=0,drops=0,lost=0;};// lost: frames dropped by a full inbox
void radioLinkScan(uint32_t ms);// fills the peer list (radioLinkPeers); the phone companion server pauses while the link is in use
std::vector<RadioLinkPeer> radioLinkPeers();
void radioLinkConnect(const RadioLinkPeer &peer,uint32_t pin);// pairing uses pin (the companion's BLE PIN)
void radioLinkDisconnect(bool resumeServer=true);// also ends a scan or a connect; resumeServer: the phone companion server comes back
void radioLinkForgetBond(const String &address,uint8_t type);// the pairing keys of a forgotten companion
uint32_t radioLinkRevision();// cheap: changes with every status / peer change
bool radioLinkHoldsStack();// the link has a NimBLE client: the stack must not be torn down under it
RadioLinkStatus radioLinkStatus();
bool radioLinkTake(uint8_t *frame,size_t &len);
bool radioLinkSend(const uint8_t *frame,size_t len);
}
