#if !MM_EMULATOR
#include "companion_ble.h"
#include "connectivity.h"
#include <NimBLEDevice.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <algorithm>
#include <atomic>
namespace device {
namespace {
constexpr const char *NusService="6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char *NusRx="6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char *NusTx="6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr size_t FrameMax=184;
struct Frame {uint16_t len;uint8_t data[FrameMax];};
// Inbox: a ring of RingSize frames in PSRAM filled by the NimBLE host task (a contact listing arrives as a burst of
// notifies faster than one UI loop), emptied by the main loop; a full ring drops the newest frame and counts it.
constexpr int RingSize=512;Frame *ring=nullptr;int ringHead=0,ringCount=0;uint32_t ringDropped=0;portMUX_TYPE ringMux=portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t lock=nullptr;TaskHandle_t task=nullptr;
RadioLinkStatus status;std::vector<RadioLinkPeer> peers;RadioLinkPeer target;uint32_t linkPin=0;std::atomic<uint32_t> revision{0};
// One client for the whole run (created on the first connect, never deleted): the UI may cancel or disconnect it
// while the worker task connects, without a client freed under it. wanted is the UI's will: every blocking step of
// the worker checks it again, so Off / Forget during a connect is honoured.
NimBLEClient *client=nullptr;std::atomic<NimBLERemoteCharacteristic*> rx{nullptr};std::atomic<bool> wanted{false},expectingDrop{false};
// Writes go through a queue to their own task: a companion whose RX characteristic wants write-with-response (ESP32
// MeshCore builds) would otherwise block the UI loop for a connection interval or two per frame.
QueueHandle_t outbox=nullptr;TaskHandle_t sender=nullptr;SemaphoreHandle_t writing=nullptr;/* held around each write: a reconnect frees the characteristics */
struct Hold {Hold(){xSemaphoreTake(lock,portMAX_DELAY);}~Hold(){xSemaphoreGive(lock);}};
void setPhase(RadioLinkPhase phase,const String &message){Hold h;status.phase=phase;status.message=message;status.revision++;revision=status.revision;}
RadioLinkPhase phaseNow(){Hold h;return status.phase;}
void dropInbox(){portENTER_CRITICAL(&ringMux);ringHead=ringCount=0;portEXIT_CRITICAL(&ringMux);}
class ScanCallbacks:public NimBLEScanCallbacks {
 // MeshCore companions advertise the UART service; their names start with "MeshCore-" (also accepted alone).
 // The list is kept strongest first (the page shows the top rows).
 void onResult(const NimBLEAdvertisedDevice *d)override{
  std::string name=d->getName();if(!d->isAdvertisingService(NimBLEUUID(NusService))&&name.rfind("MeshCore-",0)!=0)return;
  RadioLinkPeer p;p.name=name.empty()?String(d->getAddress().toString().c_str()):String(name.c_str());p.address=d->getAddress().toString().c_str();p.addressType=d->getAddress().getType();p.rssi=d->getRSSI();
  Hold h;bool found=false;for(auto &k:peers)if(k.address==p.address){if(name.size())k.name=p.name;k.rssi=p.rssi;found=true;break;}
  if(!found){if(peers.size()>=12)return;peers.push_back(p);}
  std::stable_sort(peers.begin(),peers.end(),[](const RadioLinkPeer &a,const RadioLinkPeer &b){return a.rssi>b.rssi;});status.revision++;revision=status.revision;}
 void onScanEnd(const NimBLEScanResults&,int)override{if(phaseNow()==RadioLinkPhase::Scanning){size_t n;{Hold h;n=peers.size();}setPhase(RadioLinkPhase::Off,n?String(int(n))+" found":"No companion found");}}
} scanCallbacks;
class ClientCallbacks:public NimBLEClientCallbacks {
 void onPassKeyEntry(NimBLEConnInfo &info)override{NimBLEDevice::injectPassKey(info,linkPin);}
 void onConfirmPasskey(NimBLEConnInfo &info,uint32_t)override{NimBLEDevice::injectConfirmPasskey(info,true);}
 // Only a live link is reported here: during a connect the worker reports its own failure (pairing, service...).
 void onDisconnect(NimBLEClient*,int reason)override{rx=nullptr;if(expectingDrop.exchange(false))return;/* our own drop before a reconnect */auto was=phaseNow();if(was==RadioLinkPhase::Connected){Hold h;status.drops++;}
  if(was==RadioLinkPhase::Connected)setPhase(wanted?RadioLinkPhase::Failed:RadioLinkPhase::Off,wanted?String("Disconnected (")+reason+")":String("Off"));}
} clientCallbacks;
void notified(NimBLERemoteCharacteristic*,uint8_t *data,size_t n,bool){if(!ring||!n||n>FrameMax)return;portENTER_CRITICAL(&ringMux);
 if(ringCount==RingSize)ringDropped++;else{Frame &f=ring[(ringHead+ringCount)%RingSize];f.len=uint16_t(n);memcpy(f.data,data,n);ringCount++;}portEXIT_CRITICAL(&ringMux);}
void ensureStack(){
 if(!lock)lock=xSemaphoreCreateMutex();if(!ring)ring=static_cast<Frame*>(heap_caps_calloc(RingSize,sizeof(Frame),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(!outbox)outbox=xQueueCreate(8,sizeof(Frame));if(!writing)writing=xSemaphoreCreateMutex();
 if(!NimBLEDevice::isInitialized())NimBLEDevice::init("MatterMesh");
 bleServerPause(true);// the phone companion server steps aside: one BLE role at a time, keyboard pairing
 NimBLEDevice::setSecurityAuth(true,true,true);}
bool fail(const String &message){if(client&&client->isConnected())client->disconnect();rx=nullptr;setPhase(wanted?RadioLinkPhase::Failed:RadioLinkPhase::Off,wanted?message:String("Off"));return false;}
bool connectNow(){
 RadioLinkPeer peer;{Hold h;peer=target;}
 // Already linked to this companion (a second connect request): nothing to do.
 if(client&&client->isConnected()&&rx&&std::string(client->getPeerAddress().toString())==peer.address.c_str()){setPhase(RadioLinkPhase::Connected,"Connected");return true;}
 setPhase(RadioLinkPhase::Connecting,"Connecting...");if(NimBLEDevice::getScan()->isScanning())NimBLEDevice::getScan()->stop();rx=nullptr;
 if(!client){client=NimBLEDevice::createClient();if(!client)return fail("No BLE client");client->setClientCallbacks(&clientCallbacks,false);client->setConnectTimeout(10000);}
 if(client->isConnected()){
  expectingDrop=true;if(!client->disconnect())expectingDrop=false;for(int i=0;i<60&&client->isConnected();i++)vTaskDelay(pdMS_TO_TICKS(50));expectingDrop=false;}// the drop is asynchronous
 if(!wanted)return fail("Off");
 if(!ring||!outbox)return fail("No memory for the link");
 xSemaphoreTake(writing,portMAX_DELAY);xSemaphoreGive(writing);/* no write still running on the old characteristics */
 if(!client->connect(NimBLEAddress(std::string(peer.address.c_str()),peer.addressType)))return fail("Companion not reachable");
 if(!wanted)return fail("Off");
 setPhase(RadioLinkPhase::Connecting,"Pairing...");
 if(!client->secureConnection())return fail("Pairing failed: check the PIN");
 if(!wanted)return fail("Off");
 auto *service=client->getService(NusService);if(!service)return fail("No companion service");
 auto *tx=service->getCharacteristic(NusTx);auto *in=service->getCharacteristic(NusRx);
 if(!tx||!in||!tx->canNotify())return fail("Companion service incomplete");
 dropInbox();xQueueReset(outbox);if(!tx->subscribe(true,notified))return fail("Notify refused");
 if(!wanted)return fail("Off");
 rx=in;{Hold h;status.connects++;status.name=peer.name;status.address=peer.address;}setPhase(RadioLinkPhase::Connected,"Connected");return true;}
void worker(void*){for(;;){ulTaskNotifyTake(pdTRUE,portMAX_DELAY);if(wanted)connectNow();}}
void sending(void*){Frame f;for(;;){if(xQueueReceive(outbox,&f,portMAX_DELAY)!=pdTRUE)continue;xSemaphoreTake(writing,portMAX_DELAY);auto *in=rx.load();if(in)in->writeValue(f.data,f.len,!in->canWriteNoResponse());xSemaphoreGive(writing);}}
}
void radioLinkScan(uint32_t ms){if(lock){auto p=phaseNow();if(p==RadioLinkPhase::Connecting||p==RadioLinkPhase::Connected)return;}ensureStack();{Hold h;peers.clear();}setPhase(RadioLinkPhase::Scanning,"Searching...");
 auto *scan=NimBLEDevice::getScan();scan->setScanCallbacks(&scanCallbacks,false);scan->setActiveScan(true);scan->setMaxResults(0);/* callbacks only: no result list in internal RAM */if(!scan->start(ms,false,true))setPhase(RadioLinkPhase::Failed,"Scan failed");}
std::vector<RadioLinkPeer> radioLinkPeers(){if(!lock)return {};Hold h;return peers;}
void radioLinkConnect(const RadioLinkPeer &peer,uint32_t pin){ensureStack();{Hold h;target=peer;linkPin=pin;}wanted=true;
 if(!task&&xTaskCreatePinnedToCore(worker,"mmlink",6144,nullptr,1,&task,0)!=pdPASS){task=nullptr;setPhase(RadioLinkPhase::Failed,"No memory for the link");return;}
 if(!sender&&outbox&&xTaskCreatePinnedToCore(sending,"mmlinktx",4096,nullptr,1,&sender,0)!=pdPASS){sender=nullptr;setPhase(RadioLinkPhase::Failed,"No memory for the link");return;}
 xTaskNotifyGive(task);}
void radioLinkDisconnect(bool resumeServer){wanted=false;if(!lock)return;if(NimBLEDevice::getScan()->isScanning())NimBLEDevice::getScan()->stop();
 rx=nullptr;if(client){client->cancelConnect();if(client->isConnected())client->disconnect();}dropInbox();if(outbox)xQueueReset(outbox);setPhase(RadioLinkPhase::Off,"Off");if(resumeServer)bleServerPause(false);}
void radioLinkForgetBond(const String &address,uint8_t type){if(!lock||!address.length())return;NimBLEDevice::deleteBond(NimBLEAddress(std::string(address.c_str()),type));}
RadioLinkStatus radioLinkStatus(){if(!lock)return {};Hold h;RadioLinkStatus s=status;s.lost=ringDropped;return s;}
uint32_t radioLinkRevision(){return revision;}
bool radioLinkTake(uint8_t *frame,size_t &len){if(!ring)return false;bool got=false;portENTER_CRITICAL(&ringMux);
 if(ringCount){Frame &f=ring[ringHead];memcpy(frame,f.data,f.len);len=f.len;ringHead=(ringHead+1)%RingSize;ringCount--;got=true;}portEXIT_CRITICAL(&ringMux);return got;}
bool radioLinkSend(const uint8_t *frame,size_t len){if(!rx.load()||!outbox||!len||len>FrameMax)return false;Frame f;f.len=uint16_t(len);memcpy(f.data,frame,len);return xQueueSend(outbox,&f,0)==pdTRUE;}
bool radioLinkHoldsStack(){return client!=nullptr;}
}
#endif
