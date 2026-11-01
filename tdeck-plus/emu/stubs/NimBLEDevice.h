#pragma once
// Emulator Bluetooth (NimBLE-Arduino 2.x surface used by services/connectivity.cpp). The stack starts and
// "advertises", but no central ever connects: the MeshCore app reaches the emulator over TCP 5000 instead.
#include <Arduino.h>
#include <string>
#include <vector>
#include <memory>
#define BLE_HS_IO_DISPLAY_ONLY 0
#define BLE_HS_IO_KEYBOARD_ONLY 2
#define BLE_HS_ADV_F_DISC_GEN 0x02
#define BLE_HS_ADV_F_BREDR_UNSUP 0x04
namespace NIMBLE_PROPERTY {enum : uint16_t {READ=0x0002,WRITE_NR=0x0004,WRITE=0x0008,NOTIFY=0x0010,INDICATE=0x0020,READ_ENC=0x0200,READ_AUTHEN=0x0400,WRITE_ENC=0x1000,WRITE_AUTHEN=0x2000};}
class NimBLEConnInfo {public:uint16_t getConnHandle() const {return 0;}bool isEncrypted() const {return true;}};
class NimBLEAttValue {
 std::vector<uint8_t> bytes;
public:
 NimBLEAttValue()=default;NimBLEAttValue(const uint8_t *d,size_t n):bytes(d,d+n){}
 const uint8_t *data() const {return bytes.data();}size_t size() const {return bytes.size();}size_t length() const {return bytes.size();}
};
class NimBLECharacteristic;
class NimBLECharacteristicCallbacks {
public:
 virtual ~NimBLECharacteristicCallbacks(){}
 virtual void onWrite(NimBLECharacteristic*,NimBLEConnInfo&){}
 virtual void onSubscribe(NimBLECharacteristic*,NimBLEConnInfo&,uint16_t){}
};
class NimBLECharacteristic {
 NimBLEAttValue value;NimBLECharacteristicCallbacks *callbacks=nullptr;
public:
 void setCallbacks(NimBLECharacteristicCallbacks *c){callbacks=c;}
 void setValue(const uint8_t *d,size_t n){value=NimBLEAttValue(d,n);}
 void setValue(const char *s){value=NimBLEAttValue(reinterpret_cast<const uint8_t*>(s),strlen(s));}
 void setValue(const String &s){setValue(s.c_str());}
 NimBLEAttValue getValue() const {return value;}
 bool notify(){return false;}// nobody subscribed
};
class NimBLEService {
 std::vector<std::unique_ptr<NimBLECharacteristic>> characteristics;
public:
 NimBLECharacteristic *createCharacteristic(const char*,uint32_t){characteristics.emplace_back(new NimBLECharacteristic());return characteristics.back().get();}
 bool start(){return true;}
};
class NimBLEServer;
class NimBLEServerCallbacks {
public:
 virtual ~NimBLEServerCallbacks(){}
 virtual void onConnect(NimBLEServer*,NimBLEConnInfo&){}
 virtual void onDisconnect(NimBLEServer*,NimBLEConnInfo&,int){}
 virtual uint32_t onPassKeyDisplay(){return 123456;}
 virtual void onAuthenticationComplete(NimBLEConnInfo&){}
};
class NimBLEServer {
 std::vector<std::unique_ptr<NimBLEService>> services;NimBLEServerCallbacks *callbacks=nullptr;
public:
 void setCallbacks(NimBLEServerCallbacks *c,bool=true){callbacks=c;}
 void advertiseOnDisconnect(bool){}
 NimBLEService *createService(const char*){services.emplace_back(new NimBLEService());return services.back().get();}
 bool disconnect(uint16_t,uint8_t=0x13){return true;}
 std::vector<uint16_t> getPeerDevices() const {return {};}
};
class NimBLEAdvertisementData {
public:
 void setFlags(uint8_t){}void addServiceUUID(const char*){}void setName(const char*){}
};
class NimBLEAdvertising {
 bool running=false;
public:
 void setAdvertisementData(const NimBLEAdvertisementData&){}void setScanResponseData(const NimBLEAdvertisementData&){}
 bool start(uint32_t=0){running=true;return true;}
 bool stop(){running=false;return true;}
 bool isAdvertising() const {return running;}
};
class NimBLEDevice {
 static NimBLEServer *&serverSlot(){static NimBLEServer *s=nullptr;return s;}
 static NimBLEAdvertising &advertising(){static NimBLEAdvertising a;return a;}
public:
 static bool init(const char*){return true;}
 static bool deinit(bool=false){delete serverSlot();serverSlot()=nullptr;advertising().stop();return true;}
 static bool setMTU(uint16_t){return true;}
 static void setSecurityAuth(bool,bool,bool){}
 static void setSecurityIOCap(uint8_t){}
 static void setSecurityPasskey(uint32_t){}
 static bool setDeviceName(const std::string&){return true;}// renamed live by bleCompanionCustomName
 static NimBLEServer *createServer(){if(!serverSlot())serverSlot()=new NimBLEServer();return serverSlot();}
 static NimBLEServer *getServer(){return serverSlot();}
 static NimBLEAdvertising *getAdvertising(){return &advertising();}
 static bool stopAdvertising(){return advertising().stop();}
};
