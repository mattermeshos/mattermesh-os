#pragma once
#include <Arduino.h>
#include <vector>
namespace device {
// Background network work (weather, storm alerts, OTA, map tile downloads, link viewer). While any runs, a locked Plus keeps the full CPU clock: at the
// 80 MHz locked clock (octal PSRAM floor) their TCP/TLS connections time out ("HTTP -1"; fine at 240 MHz). The core-0
// task watchdog is paused meanwhile (slow PSRAM TLS handshakes, connectivity.cpp).
void networkJobBegin();void networkJobEnd();bool networkJobActive();
struct Network {String ssid;int rssi=0;bool protectedNetwork=true,saved=false;};
struct AccessPoint {uint8_t bssid[6]={};int rssi=0;};
struct ConnectivityStatus {bool reconnectPaused=false,wifi=true,connected=false,connecting=false,scanning=false,ble=true,bleReady=false,bleClient=false,remembered=false;int rssi=0;String ssid,address,message;uint32_t revision=0;size_t networkCount=0;};
struct ConnectivityState:ConnectivityStatus {std::vector<Network> networks;ConnectivityStatus status()const{ConnectivityStatus view=*this;view.networkCount=networks.size();return view;}};
uint32_t connectivityRevision();bool wifiConnected();
ConnectivityStatus connectivityStatus();
// Password of a network joined before (known networks, newest first); false if it was never joined.
bool knownNetwork(const String &ssid,String &password);
// Names of the known networks (newest first) and forgetting one (password wiped, NVS updated; false if unknown).
std::vector<String> savedNetworks();bool forgetNetwork(const String &ssid);
// MeshCore companion app link over BLE (Nordic UART service, passkey pairing). Frames up to 176 bytes.
constexpr size_t BLE_COMPANION_FRAME_MAX=176;
void bleCompanionConfig(const String &nodeName,uint32_t pin);// call before bleEnabled(); name becomes "MeshCore-<node name>"
// User-chosen BLE name after "MeshCore-" (empty: the node name); applied at once, also while advertising.
void bleCompanionCustomName(const String &name);constexpr unsigned BLE_NAME_MAX_BYTES=20;
void bleServerPause(bool pause);// the companion link (companion_ble.cpp) takes BLE: no advertising, phone dropped, keyboard pairing
bool bleCompanionTake(uint8_t *frame,size_t &len);bool bleCompanionSend(const uint8_t *frame,size_t len);bool bleCompanionConnected();
String bleCompanionName();
void connectivityLowPower(bool locked);void connectivityBegin();void connectivityLoop();ConnectivityState connectivityState();void wifiEnabled(bool enabled,bool persist=true);/* persist=false: temporary switch (weather lookup, boot restore) */void bleEnabled(bool enabled,bool boot=false);void scan();uint32_t wifiScanCount();std::vector<AccessPoint> wifiAccessPoints();void reconnect();bool connect(const String &ssid,const String &password);void disconnect();
}
