#include "clock_limits.h"
#include "connectivity.h"
#include "wifi_policy.h"
#include <atomic>
#include <algorithm>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <time.h>
#include "ui/build_info.h"
namespace device {
static Preferences preferences;static ConnectivityState state;static uint32_t attemptAt=0,retryAt=0;static String savedSSID,savedPassword;static bool bleInitialized=false;static NimBLEServer *server=nullptr;static std::atomic<bool> bleConnected{false},bleRequested{false};
// MeshCore companion link over BLE: the Nordic UART service of the companion firmware (one write = one frame in,
// one notification = one frame out), bonded with a 6-digit passkey shown on Settings > Bluetooth.
static constexpr const char *NusService="6E400001-B5A3-F393-E0A9-E50E24DCCA9E",*NusRx="6E400002-B5A3-F393-E0A9-E50E24DCCA9E",*NusTx="6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
struct BleFrame {uint16_t len;uint8_t data[BLE_COMPANION_FRAME_MAX];};
static QueueHandle_t bleInbox=nullptr;static NimBLECharacteristic *bleTx=nullptr;static String bleName="MeshCore-MatterMesh";static uint32_t blePin=123456;static std::atomic<bool> bleSubscribed{false};
class Callbacks:public NimBLEServerCallbacks {
 void onConnect(NimBLEServer *server,NimBLEConnInfo &info)override{if(!bleRequested){server->disconnect(info.getConnHandle());bleConnected=false;}else bleConnected=true;}
 void onDisconnect(NimBLEServer*,NimBLEConnInfo&,int)override{bleConnected=false;bleSubscribed=false;}
 uint32_t onPassKeyDisplay()override{return blePin;}
 void onAuthenticationComplete(NimBLEConnInfo &info)override{if(!info.isEncrypted())NimBLEDevice::getServer()->disconnect(info.getConnHandle());}
};
class RxCallbacks:public NimBLECharacteristicCallbacks {
 void onWrite(NimBLECharacteristic *c,NimBLEConnInfo&)override{auto v=c->getValue();if(!bleInbox||!v.size()||v.size()>BLE_COMPANION_FRAME_MAX)return;BleFrame f;f.len=uint16_t(v.size());memcpy(f.data,v.data(),v.size());xQueueSend(bleInbox,&f,0);}
};
class TxCallbacks:public NimBLECharacteristicCallbacks {
 void onSubscribe(NimBLECharacteristic*,NimBLEConnInfo&,uint16_t sub)override{bleSubscribed=sub!=0;}
};
static RxCallbacks rxCallbacks;static TxCallbacks txCallbacks;
// BLE name: "MeshCore-" + the user's name for it (Settings > Bluetooth), else + the node name; at most 29 bytes.
static String bleNode="MatterMesh",bleCustom;
static void bleRename(){String name=String("MeshCore-")+(bleCustom.length()?bleCustom:bleNode);if(name.length()>29)name=name.substring(0,29);if(name==bleName)return;bleName=name;
 if(!bleInitialized)return;// startBLE() advertises it
 // Live: GAP device name and scan response, advertising restarted (a connected app keeps its link).
 NimBLEDevice::setDeviceName(bleName.c_str());auto advertising=NimBLEDevice::getAdvertising();bool on=advertising->isAdvertising();if(on)advertising->stop();
 NimBLEAdvertisementData scan;scan.setName(bleName.c_str());advertising->setScanResponseData(scan);if(on)advertising->start();}
void bleCompanionConfig(const String &nodeName,uint32_t pin){bleNode=nodeName;bleRename();blePin=pin;if(bleInitialized)NimBLEDevice::setSecurityPasskey(pin);}
void bleCompanionCustomName(const String &name){bleCustom=name;bleRename();}
bool bleCompanionTake(uint8_t *frame,size_t &len){BleFrame f;if(!bleInbox||xQueueReceive(bleInbox,&f,0)!=pdTRUE)return false;memcpy(frame,f.data,f.len);len=f.len;return true;}
bool bleCompanionSend(const uint8_t *frame,size_t len){if(!bleTx||!bleConnected||!bleSubscribed)return false;bleTx->setValue(frame,len);return bleTx->notify();}
bool bleCompanionConnected(){return bleConnected&&bleSubscribed;}
String bleCompanionName(){return bleName;}
static Callbacks callbacks;
static WifiRetryPolicy retryPolicy;static WifiConnectionStart connectionStart;static bool staged=false;static String pendingSSID,pendingPassword;static std::atomic<int> disconnectReason{0};static std::atomic<bool> freshIP{false};
static void wipe(String &value){for(unsigned i=0;i<value.length();i++)value.setCharAt(i,0);value="";}

static bool startBLE(){
 if(bleInitialized){server->advertiseOnDisconnect(true);return NimBLEDevice::getAdvertising()->start();}
 if(!NimBLEDevice::init(bleName.c_str()))return false;
 NimBLEDevice::setMTU(BLE_COMPANION_FRAME_MAX+3);NimBLEDevice::setSecurityAuth(true,true,true);NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);NimBLEDevice::setSecurityPasskey(blePin);
 if(!bleInbox)bleInbox=xQueueCreate(8,sizeof(BleFrame));
 server=NimBLEDevice::createServer();server->setCallbacks(&callbacks,false);server->advertiseOnDisconnect(true);
 auto info=server->createService("180a");info->createCharacteristic("2a29",NIMBLE_PROPERTY::READ)->setValue("MatterMesh OS / LilyGO");info->createCharacteristic("2a26",NIMBLE_PROPERTY::READ)->setValue(MM_VERSION);info->start();
 auto nus=server->createService(NusService);
 bleTx=nus->createCharacteristic(NusTx,NIMBLE_PROPERTY::READ|NIMBLE_PROPERTY::NOTIFY|NIMBLE_PROPERTY::READ_ENC|NIMBLE_PROPERTY::READ_AUTHEN);bleTx->setCallbacks(&txCallbacks);
 nus->createCharacteristic(NusRx,NIMBLE_PROPERTY::WRITE|NIMBLE_PROPERTY::WRITE_NR|NIMBLE_PROPERTY::WRITE_ENC|NIMBLE_PROPERTY::WRITE_AUTHEN)->setCallbacks(&rxCallbacks);nus->start();
 // Advertisement: flags + the UART service (what the MeshCore app scans for); the name goes in the scan response.
 auto advertising=NimBLEDevice::getAdvertising();NimBLEAdvertisementData adv,scan;adv.setFlags(BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP);adv.addServiceUUID(NusService);scan.setName(bleName.c_str());
 advertising->setAdvertisementData(adv);advertising->setScanResponseData(scan);
 bleInitialized=advertising->start();if(!bleInitialized){NimBLEDevice::deinit(true);server=nullptr;}return bleInitialized;
}
void bleEnabled(bool enabled){
 bleRequested=enabled;if(enabled){state.bleReady=startBLE();state.ble=state.bleReady;}
 else{if(bleInitialized){server->advertiseOnDisconnect(false);NimBLEDevice::stopAdvertising();for(auto handle:server->getPeerDevices())server->disconnect(handle);}bleConnected=false;state.ble=state.bleReady=state.bleClient=false;}
 preferences.putBool("ble",state.ble);state.revision++;
}
// Known Wi-Fi networks (newest first, up to KnownMax): every network joined once keeps its password, so it is never
// asked again and the device reconnects to whichever of them is around. NVS "networks" (JSON); the single "network"
// of older builds is read once and kept as the first entry.
constexpr size_t KnownMax=8;struct Known {String ssid,password;};static std::vector<Known> known;static size_t knownCursor=0;
static bool saveKnown(){DynamicJsonDocument doc(2048);auto list=doc.to<JsonArray>();for(auto &k:known){auto o=list.createNestedObject();o["s"]=k.ssid;o["p"]=k.password;}String encoded;serializeJson(doc,encoded);return preferences.putString("networks",encoded)==encoded.length();}
static bool saveNetwork(const String &ssid,const String &password){
 if(!known.empty()&&known[0].ssid==ssid&&known[0].password==password)return true;// already the newest: no NVS write
 for(size_t i=0;i<known.size();i++)if(known[i].ssid==ssid){known.erase(known.begin()+i);break;}
 known.insert(known.begin(),Known{ssid,password});if(known.size()>KnownMax)known.resize(KnownMax);knownCursor=0;return saveKnown();}
bool knownNetwork(const String &ssid,String &password){for(auto &k:known)if(k.ssid==ssid){password=k.password;return true;}return false;}
// Saved network for the next automatic attempt: the strongest known one in the last scan, otherwise the next one in
// the list (knownCursor moves on after each failed attempt).
static void pickSaved(){
 if(known.empty()){savedSSID="";wipe(savedPassword);return;}
 int best=-1,bestRssi=-1000;for(auto &n:state.networks)for(size_t i=0;i<known.size();i++)if(known[i].ssid==n.ssid&&n.rssi>bestRssi){best=int(i);bestRssi=n.rssi;}
 size_t i=best>=0?size_t(best):knownCursor%known.size();savedSSID=known[i].ssid;savedPassword=known[i].password;}
static void beginConnection(){
 state.connected=false;state.address="";state.connecting=true;attemptAt=millis();state.message="Connecting...";disconnectReason=0;freshIP=false;
 if(!staged)pickSaved();
 if(staged||savedSSID.length()){WiFi.disconnect(false,false);connectionStart.queue(millis());}
 else{connectionStart.cancel();state.connecting=false;state.message="Select a network";retryPolicy.disconnect();}
 state.revision++;
}
void wifiEnabled(bool enabled,bool persist){
 state.wifi=enabled;if(persist)preferences.putBool("wifi",enabled);retryPolicy.connect();
 if(enabled){WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(false);beginConnection();}
 else{connectionStart.cancel();staged=false;wipe(pendingPassword);pendingSSID="";if(state.scanning)WiFi.scanDelete();state.scanning=false;WiFi.disconnect(false,false);WiFi.mode(WIFI_OFF);state.connected=state.connecting=false;state.address="";state.message="Wi-Fi off";state.revision++;}
}
void connectivityBegin(){
 preferences.begin("mmconnect",false);
 {String encoded=preferences.isKey("networks")?preferences.getString("networks",""):String("");DynamicJsonDocument doc(2048);
  if(encoded.length()&&!deserializeJson(doc,encoded))for(JsonObject o:doc.as<JsonArray>()){Known k{o["s"].as<String>(),o["p"].as<String>()};if(validWifiCredentials(k.ssid.c_str(),k.password.c_str())&&known.size()<KnownMax)known.push_back(k);}}
 // Single-network keys of old builds (they held the password a second time): not read any more, dropped.
 for(const char *k:{"ssid","password","network"})if(preferences.isKey(k))preferences.remove(k);
 pickSaved();state.remembered=!known.empty();
 bool wifi=preferences.getBool("wifi",true),ble=preferences.getBool("ble",true);WiFi.persistent(false);
 WiFi.onEvent([](arduino_event_id_t event,arduino_event_info_t info){if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED){disconnectReason.store(info.wifi_sta_disconnected.reason);freshIP=false;}else if(event==ARDUINO_EVENT_WIFI_STA_GOT_IP)freshIP=true;});
 wifiEnabled(wifi,false);bleEnabled(ble);
}
static std::vector<AccessPoint> accessPoints;static uint32_t scanCount=0;
uint32_t wifiScanCount(){return scanCount;}
std::vector<AccessPoint> wifiAccessPoints(){return accessPoints;}
void scan(){if(!state.wifi||state.scanning)return;if(state.connecting){if(staged){state.message="Connecting / scan after attempt";state.revision++;return;}connectionStart.cancel();WiFi.disconnect(false,false);state.connecting=false;retryAt=millis();}/* an automatic retry of the saved network gives way to a scan the user asked for (it was refused for 20 s of every 50 s while the saved network is out of range) */WiFi.scanDelete();state.networks.clear();// Arduino fails an async scan after 20 x max_ms_per_chan (6 s at the default 300 ms) while a full sweep here takes ~6.5 s:
 // keep the 300 ms dwell and raise only that timeout (a protected static, reached through a derived class).
 struct ScanTimeout:WiFiScanClass{static void set(uint32_t ms){_scanTimeout=ms;}};
 int result=WiFi.scanNetworks(true,true);if(result==WIFI_SCAN_RUNNING)ScanTimeout::set(15000);state.scanning=result==WIFI_SCAN_RUNNING;state.message=state.scanning?"Scanning...":"Scan could not start";state.revision++;}
bool connect(const String &ssid,const String &password){
 if(!validWifiCredentials(std::string(ssid.c_str(),ssid.length()),std::string(password.c_str(),password.length()))){state.message="SSID 1-32 / password 8-63 or empty";state.revision++;return false;}
 if(!state.wifi){state.wifi=true;preferences.putBool("wifi",true);WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(false);}
 if(state.scanning){WiFi.scanDelete();state.scanning=false;}
 retryPolicy.connect();
 if(WiFi.status()==WL_CONNECTED&&WiFi.SSID()==ssid&&WiFi.psk()==password){
  if(saveNetwork(ssid,password)){savedSSID=ssid;savedPassword=password;state.remembered=true;state.message="Connected";}else state.message="Connected / settings save failed";state.revision++;return true;
 }
 pendingSSID=ssid;pendingPassword=password;staged=true;beginConnection();return true;
}
void reconnect(){if(!state.wifi){wifiEnabled(true);return;}if(state.scanning){WiFi.scanDelete();state.scanning=false;}staged=false;wipe(pendingPassword);pendingSSID="";retryPolicy.connect();beginConnection();}
void disconnect(){connectionStart.cancel();retryPolicy.disconnect();staged=false;wipe(pendingPassword);pendingSSID="";WiFi.disconnect(false,false);state.connected=state.connecting=false;state.address="";state.message="Disconnected / auto-reconnect paused";state.revision++;}
void connectivityLoop(){
 if(connectionStart.ready(millis(),WiFi.status()==WL_CONNECTED)){
  connectionStart.cancel();attemptAt=millis();disconnectReason=0;freshIP=false;
  if(staged)WiFi.begin(pendingSSID.c_str(),pendingPassword.c_str());else WiFi.begin(savedSSID.c_str(),savedPassword.c_str());
 }else if(connectionStart.timedOut(millis())){connectionStart.cancel();attemptAt=millis()-WIFI_CONNECT_TIMEOUT_MS;}
 bool connected=state.wifi&&retryPolicy.allowed&&!connectionStart.pending&&freshIP.load()&&WiFi.status()==WL_CONNECTED&&WiFi.localIP()!=IPAddress(0,0,0,0)&&(!staged||wifiMayCommit(staged,freshIP.load(),pendingSSID.c_str(),WiFi.SSID().c_str()));// no SSID String per loop unless staged
 if(connected!=state.connected){state.connected=connected;state.connecting=false;state.message=connected?"Connected":"Disconnected";
  if(connected){state.ssid=WiFi.SSID();state.address=WiFi.localIP().toString();state.rssi=WiFi.RSSI();
   if(staged){if(saveNetwork(pendingSSID,pendingPassword)){savedSSID=pendingSSID;savedPassword=pendingPassword;state.remembered=true;}else state.message="Connected / settings save failed";staged=false;wipe(pendingPassword);pendingSSID="";}
   configTzTime(MM_TZ,"pool.ntp.org","time.google.com");
  }else{state.address="";retryAt=millis();}state.revision++;
 }
 int reason=disconnectReason.load();bool authentication=reason==WIFI_REASON_AUTH_FAIL||reason==WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT||reason==WIFI_REASON_HANDSHAKE_TIMEOUT;
 bool failed=state.connecting&&((uint32_t(millis()-attemptAt)>=500&&(authentication||WiFi.status()==WL_CONNECT_FAILED))||uint32_t(millis()-attemptAt)>=WIFI_CONNECT_TIMEOUT_MS);
 if(failed){connectionStart.cancel();state.connecting=false;state.message=authentication?"Authentication failed / check password":reason==WIFI_REASON_NO_AP_FOUND?"Network not found":"Connection failed / retry";if(staged){retryPolicy.disconnect();staged=false;wipe(pendingPassword);pendingSSID="";}else knownCursor++;retryAt=millis();state.revision++;WiFi.disconnect(false,false);}
 if(retryPolicy.due(state.wifi,state.connected,state.connecting,state.scanning,millis(),retryAt))beginConnection();
 if(state.scanning){int count=WiFi.scanComplete();
  if(count>=0){
   // Strongest BSSIDs for Wi-Fi positioning (weather); "_nomap" networks opted out.
   accessPoints.clear();for(int i=0;i<count&&accessPoints.size()<16;i++){if(WiFi.SSID(i).endsWith("_nomap"))continue;AccessPoint ap;memcpy(ap.bssid,WiFi.BSSID(i),6);ap.rssi=WiFi.RSSI(i);accessPoints.push_back(ap);}scanCount++;
   for(int i=0;i<count&&state.networks.size()<24;i++){String ssid=WiFi.SSID(i);if(!ssid.length())continue;bool duplicate=false;for(auto &n:state.networks)if(n.ssid==ssid){duplicate=true;if(WiFi.RSSI(i)>n.rssi){n.rssi=WiFi.RSSI(i);n.protectedNetwork=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;}}if(!duplicate){Network n;n.ssid=ssid;n.rssi=WiFi.RSSI(i);n.protectedNetwork=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;state.networks.push_back(n);}}
   for(auto &n:state.networks){String password;n.saved=knownNetwork(n.ssid,password);wipe(password);}
   std::sort(state.networks.begin(),state.networks.end(),[](const Network &a,const Network &b){return a.rssi>b.rssi;});state.scanning=false;WiFi.scanDelete();state.message=String(state.networks.size())+" networks found";state.revision++;
  }else if(count==WIFI_SCAN_FAILED){state.scanning=false;WiFi.scanDelete();state.message="Scan failed / retry";state.revision++;}
 }
 if(bleConnected!=state.bleClient){state.bleClient=bleConnected;state.revision++;}
}
uint32_t connectivityRevision(){return state.revision;}
bool wifiConnected(){return state.connected;}
bool wifiOn(){return state.wifi;}
bool bleOn(){return state.bleReady;}
ConnectivityState connectivityState(){auto view=state;view.reconnectPaused=!retryPolicy.allowed;return view;}
ConnectivityStatus connectivityStatus(){auto view=state.status();view.reconnectPaused=!retryPolicy.allowed;return view;}
void connectivityLowPower(bool locked){
 static bool suspended=false,oldSleep=false;
 if(locked){oldSleep=WiFi.getSleep();if(state.wifi)WiFi.setSleep(true);if(state.bleReady&&!state.bleClient){NimBLEDevice::getAdvertising()->stop();suspended=true;}}
 else {if(state.wifi)WiFi.setSleep(oldSleep);if(suspended&&state.ble)NimBLEDevice::getAdvertising()->start();suspended=false;}
}

}
