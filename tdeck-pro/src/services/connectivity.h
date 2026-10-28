#pragma once
#include <Arduino.h>
#include <vector>
namespace device {
struct Network {String ssid;int rssi=0;bool protectedNetwork=true,saved=false;};
struct AccessPoint {uint8_t bssid[6]={};int rssi=0;};
struct ConnectivityStatus {bool reconnectPaused=false,wifi=true,connected=false,connecting=false,scanning=false,ble=true,bleReady=false,bleClient=false,remembered=false;int rssi=0;String ssid,address,message;uint32_t revision=0;size_t networkCount=0;};
struct ConnectivityState:ConnectivityStatus {std::vector<Network> networks;ConnectivityStatus status()const{ConnectivityStatus view=*this;view.networkCount=networks.size();return view;}};
uint32_t connectivityRevision();bool wifiConnected();bool wifiOn();bool bleOn();/* cheap reads, no state copy */
ConnectivityStatus connectivityStatus();
// Password of a network joined before (known networks, newest first); false if it was never joined.
bool knownNetwork(const String &ssid,String &password);
// MeshCore companion app link over BLE (Nordic UART service, passkey pairing). Frames up to 176 bytes.
constexpr size_t BLE_COMPANION_FRAME_MAX=176;
void bleCompanionConfig(const String &nodeName,uint32_t pin);// call before bleEnabled(); name becomes "MeshCore-<node name>"
// User-chosen BLE name after "MeshCore-" (empty: the node name); applied at once, also while advertising.
void bleCompanionCustomName(const String &name);constexpr unsigned BLE_NAME_MAX_BYTES=20;
bool bleCompanionTake(uint8_t *frame,size_t &len);bool bleCompanionSend(const uint8_t *frame,size_t len);bool bleCompanionConnected();
String bleCompanionName();
void connectivityLowPower(bool locked);void connectivityBegin();void connectivityLoop();ConnectivityState connectivityState();void wifiEnabled(bool enabled,bool persist=true);/* persist=false: temporary switch (weather lookup, boot restore) */void bleEnabled(bool enabled);void scan();uint32_t wifiScanCount();std::vector<AccessPoint> wifiAccessPoints();void reconnect();bool connect(const String &ssid,const String &password);void disconnect();
}
