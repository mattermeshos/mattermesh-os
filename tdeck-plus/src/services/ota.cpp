#include "ota.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_heap_caps.h>
#include <mbedtls/sha256.h>
#include <memory>
#include "ed_25519.h"
#include "ui/build_info.h"
#include "services/hex_util.h"
#include "services/connectivity.h"
#include <SD.h>

// The Arduino core marks every image valid at boot unless this returns true; otaLoop() does it once healthy.
extern "C" bool verifyRollbackLater(){return true;}

namespace device {
// Public half of ~/.config/mattermesh/ota/signing.key (tools/ota_publish.py keygen).
static const uint8_t SigningKey[32]={0xf6,0x44,0x54,0xa5,0x7d,0x17,0x66,0xd2,0xe5,0x45,0x59,0xa7,0x0a,0xb5,0xe3,0xc4,0x55,0x7a,0x15,0x78,0xef,0x82,0x7f,0x20,0xfd,0x78,0x4c,0x9a,0x25,0x53,0x75,0x0a};
// Default servers come from the build (tools/ota_default_servers.py); none when the private list is missing.
#ifndef MM_OTA_DEFAULT_SERVERS
#define MM_OTA_DEFAULT_SERVERS ""
#endif
static const char *DefaultServers=MM_OTA_DEFAULT_SERVERS;
// Public half of ~/.config/mattermesh/ota/license.key (apps licenses, docs/unlock-protocol.md).
static const uint8_t LicenseKey[32]={0x4f,0x30,0xf1,0x95,0x9a,0xd3,0x8e,0xa8,0x31,0xc1,0xe0,0x56,0x90,0x54,0xde,0xba,0x5d,0xdc,0xe8,0x6d,0x1e,0x46,0xc9,0xb3,0xa9,0xe5,0x90,0xb0,0x80,0x40,0xfe,0x1e};
// Edition sent with every check (docs/unlock-protocol.md). A Free image is offered the Full one once the server holds
// a license for this device; Full images are signed for "<board>-full".
static const char *Edition="free";
static Preferences preferences;static OtaState state;static SemaphoreHandle_t mutex=nullptr;static bool busy=false,healthy=false,checkedOnce=false,launcherHosted=false;
// rejected: a build that rolled back twice is no longer offered (one rollback can be a power cut in the first 30 s).
// offerEdition: edition of the release on offer, saved with "try" so a Free -> Full switch that rolls back is seen.
static uint32_t connectedAt=0,lastCheck=0,restartAt=0;static int32_t rejected=0;static String servers,pendingReport,pendingDetail,sha256,signature,path,license,offerEdition,rejectedEdition;
struct Report {String server,status,detail;int32_t code;};

const char *otaBoard(){
 return "plus";
}
static String deviceId(){uint8_t mac[6];WiFi.macAddress(mac);return String(mm::hexString(mac,6,true).c_str());}
// Apps unlock code (docs/unlock-protocol.md): SHA-256("MMID1\n"+dev), first 40 bits as 8 Crockford base32 digits, XXXX-XXXX.
// The customer writes it in the Ko-fi order; the server finds the device by it.
String otaUnlockCode(){
 static const char digits[]="0123456789ABCDEFGHJKMNPQRSTVWXYZ";String text="MMID1\n"+deviceId();uint8_t h[32];
 mbedtls_sha256_context ctx;mbedtls_sha256_init(&ctx);mbedtls_sha256_starts(&ctx,0);mbedtls_sha256_update(&ctx,(const unsigned char*)text.c_str(),text.length());mbedtls_sha256_finish(&ctx,h);mbedtls_sha256_free(&ctx);uint64_t v=0;for(int i=0;i<5;i++)v=v<<8|h[i];
 char code[10];for(int i=0,at=0;i<8;i++){if(i==4)code[at++]='-';code[at++]=digits[(v>>(35-5*i))&31];}code[9]=0;return code;
}
static bool hexBytes(const String &hex,uint8_t *out,size_t length);
// {"dev","board","issued","sig"}: sig = ed25519 over "MMLIC1\n"+dev+"\n"+board+"\nfull\n"+issued, for this T-Deck only.
static bool licenseValid(const String &json){
 StaticJsonDocument<384> doc;if(!json.length()||deserializeJson(doc,json))return false;
 String dev=doc["dev"]|"",board=doc["board"]|"",sig=doc["sig"]|"";uint32_t issued=doc["issued"]|0;uint8_t raw[64];
 if(dev!=deviceId()||board!=otaBoard()||!issued||!hexBytes(sig,raw,64))return false;
 String message="MMLIC1\n"+dev+"\n"+board+"\nfull\n"+String(issued);
 return ed25519_verify(raw,(const uint8_t*)message.c_str(),message.length(),LicenseKey)==1;
}
bool otaAppsUnlocked(){
 return false;// the Free image has no apps to unlock: it installs the Full one instead
}
static void publish(){state.revision++;}
static void setPhase(OtaPhase phase,const String &message){xSemaphoreTake(mutex,portMAX_DELAY);state.phase=phase;state.message=message;publish();xSemaphoreGive(mutex);}
static bool secureUrl(const String &url){return url.startsWith("https://");}
// TLS only hides the traffic: authenticity comes from the ed25519 signature, so the certificate is not pinned.
static bool open(HTTPClient &http,std::unique_ptr<WiFiClient> &client,const String &url){
 if(secureUrl(url)){auto secure=new WiFiClientSecure;secure->setInsecure();secure->setHandshakeTimeout(10);client.reset(secure);}else client.reset(new WiFiClient);
 http.useHTTP10(true);http.setConnectTimeout(5000);http.setTimeout(15000);http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);return http.begin(*client,url);
}
static bool hexBytes(const String &hex,uint8_t *out,size_t length){
 return mm::parseHex(hex.c_str(),hex.length(),out,length);
}
static bool signatureValid(const String &board,const String &version,int32_t code,const String &sha,const String &sig){
 uint8_t raw[64];if(!hexBytes(sig,raw,64)||sha.length()!=64)return false;
 String message="MMOTA1\n"+board+"\n"+version+"\n"+String(code)+"\n"+sha;
 return ed25519_verify(raw,(const uint8_t*)message.c_str(),message.length(),SigningKey)==1;
}

static void sendReport(const Report &report){
 std::unique_ptr<WiFiClient> client;HTTPClient http;if(!open(http,client,report.server+"/api/v1/report"))return;
 StaticJsonDocument<256> body;body["dev"]=deviceId();body["board"]=otaBoard();body["code"]=report.code;body["status"]=report.status;body["detail"]=report.detail;
 String payload;serializeJson(body,payload);http.addHeader("Content-Type","application/json");http.POST(payload);http.end();
}

static void checkWorker(void*){
 xSemaphoreTake(mutex,portMAX_DELAY);bool hadOffer=state.phase==OtaPhase::Available;String offerMessage=state.message;xSemaphoreGive(mutex);
 setPhase(OtaPhase::Checking,"Checking...");
 xSemaphoreTake(mutex,portMAX_DELAY);String list=servers+" ";int32_t bad=rejected;xSemaphoreGive(mutex);int start=0;bool reached=false,answered=false;
 while(start<int(list.length())){
  int end=list.indexOf(' ',start);String base=list.substring(start,end);start=end+1;if(!base.length())continue;
  std::unique_ptr<WiFiClient> client;HTTPClient http;
  String url=base+"/api/v1/check?board="+otaBoard()+"&code="+String(MM_VERSION_CODE)+"&dev="+deviceId()+"&ver="+MM_BUILD_VERSION+"&ed="+Edition;
  if(!open(http,client,url))continue;int status=http.GET();
  if(status!=200){http.end();continue;}
  answered=true;DynamicJsonDocument doc(1536);auto error=deserializeJson(doc,http.getStream());http.end();if(error)continue;
  reached=true;
  if(pendingReport.length()){Report report{base,pendingReport,pendingDetail,MM_VERSION_CODE};sendReport(report);pendingReport="";}
  // Licensed devices get their license with every answer: the Free image keeps it for the Full one it installs, a Full
  // image without one (first boot after a lost NVS) picks it up here. A missing license never removes a stored one.
  {String fresh;if(doc["license"].is<JsonObject>())serializeJson(doc["license"],fresh);
   if(licenseValid(fresh)){xSemaphoreTake(mutex,portMAX_DELAY);bool changed=license!=fresh;license=fresh;xSemaphoreGive(mutex);
    if(changed)preferences.putString("license",fresh);
   }}
  if(!doc["update"].as<bool>()){
   xSemaphoreTake(mutex,portMAX_DELAY);state.phase=OtaPhase::UpToDate;state.message="Up to date";state.server=base;state.checkedAt=millis();publish();xSemaphoreGive(mutex);break;
  }
  String version=doc["version"]|"",sha=doc["sha256"]|"",sig=doc["sig"]|"",file=doc["path"]|"";int32_t code=doc["code"]|0;uint32_t size=doc["size"]|0;
  // With a single OTA slot (the Launcher's layout) ESP-IDF hands back the running one, and Update would erase the
  // image it runs from: that is never a slot. Under the Launcher the image goes to the SD card instead (no slot needed).
  const esp_partition_t *slot=esp_ota_get_next_update_partition(nullptr);if(slot==esp_ota_get_running_partition())slot=nullptr;const bool toSd=launcherHosted;
  // Free -> Full is offered at the same build number too: the edition is what changes, never an older build.
  bool full=String(doc["edition"]|"")=="full",switching=full&&String(Edition)=="free";String signedBoard=String(otaBoard())+(full?"-full":"");
  String problem=String(doc["board"]|"")!=otaBoard()?"Release for another board":code<MM_VERSION_CODE||(code==MM_VERSION_CODE&&!switching)?"Server offers an older build":code==bad&&(!rejectedEdition.length()||rejectedEdition==(full?"full":"free"))?"Build "+String(code)+" rolled back / ignored":size==0?"Release without a size":!toSd&&!slot?"No idle update slot":!toSd&&size>slot->size?"Release larger than the update slot":!file.startsWith("/fw/")?"Bad download path":!signatureValid(signedBoard,version,code,sha,sig)?"Signature not valid / ignored":"";
  xSemaphoreTake(mutex,portMAX_DELAY);state.server=base;state.checkedAt=millis();
  if(problem.length()){state.phase=OtaPhase::Error;state.message=problem;}
  else{state.phase=OtaPhase::Available;state.version=version;state.code=code;state.size=size;state.done=0;state.toSd=toSd;state.notes=doc["notes"]|"";state.message=version+(full?" Full":"")+" available";sha256=sha;signature=sig;path=file;offerEdition=full?"full":"free";}
  publish();xSemaphoreGive(mutex);break;
 }
 // Not reached: an offer found earlier stays (it is still signed and valid); otherwise say what went wrong.
 if(!reached){if(hadOffer)setPhase(OtaPhase::Available,offerMessage);else setPhase(OtaPhase::Error,answered?"Bad answer from the update server":"Update server unreachable");}
 xSemaphoreTake(mutex,portMAX_DELAY);busy=false;xSemaphoreGive(mutex);networkJobEnd();vTaskDelete(nullptr);
}

// Streams the image into the idle slot (ram: into that PSRAM buffer instead), resuming with a Range request when the connection drops.
static bool download(const String &url,uint32_t size,const String &expected,uint8_t *ram,String &problem){
 if(!ram&&!Update.begin(size,U_FLASH)){problem="Update slot unavailable";return false;}
 auto abort=[&](){if(!ram)Update.abort();};
 mbedtls_sha256_context hash;mbedtls_sha256_init(&hash);mbedtls_sha256_starts(&hash,0);
 std::unique_ptr<uint8_t[]> buffer(new uint8_t[4096]);uint32_t done=0;int attempts=0;problem="Update server unreachable";
 while(done<size&&attempts<4){
  attempts++;std::unique_ptr<WiFiClient> client;HTTPClient http;if(!open(http,client,url)){delay(1000);continue;}
  if(done)http.addHeader("Range","bytes="+String(done)+"-");
  int status=http.GET();if(status!=(done?206:200)){http.end();problem="Download failed ("+String(status)+")";delay(1000);continue;}
  WiFiClient *stream=http.getStreamPtr();uint32_t lastData=millis();
  while(done<size){
   size_t available=stream->available();
   if(!available){if(!http.connected()||uint32_t(millis()-lastData)>20000)break;delay(5);continue;}
   size_t n=stream->readBytes(buffer.get(),std::min<size_t>(std::min<size_t>(available,4096),size-done));if(!n)continue;
   mbedtls_sha256_update(&hash,buffer.get(),n);
   if(ram)memcpy(ram+done,buffer.get(),n);
   else if(Update.write(buffer.get(),n)!=n){http.end();mbedtls_sha256_free(&hash);abort();problem="Flash write failed";return false;}
   done+=n;lastData=millis();
   if(done==size||done%(32*1024)<n){xSemaphoreTake(mutex,portMAX_DELAY);state.done=done;publish();xSemaphoreGive(mutex);}
  }
  http.end();if(done<size)problem="Connection lost";
 }
 if(done==size&&!ram)setPhase(OtaPhase::Verifying,"Verifying");// the SD save sets its own phase
 uint8_t digest[32];mbedtls_sha256_finish(&hash,digest);mbedtls_sha256_free(&hash);
 if(done<size){abort();return false;}
 if(expected!=mm::hexString(digest,32).c_str()){abort();problem="Checksum mismatch / discarded";return false;}
 if(ram)return true;// SD save: nothing is installed here, no boot slot changes
 // The install state goes in before end() switches the boot slot: a power cut right after it still sees the rollback.
 xSemaphoreTake(mutex,portMAX_DELAY);String edition=offerEdition;int32_t code=state.code;xSemaphoreGive(mutex);
 preferences.putString("tryed",edition);preferences.putInt("from",MM_VERSION_CODE);preferences.putInt("try",code);
 if(!Update.end()){preferences.remove("try");preferences.remove("from");preferences.remove("tryed");problem=String("Image rejected: ")+Update.errorString();return false;}
 return true;
}

// Under the Launcher the downloaded image (PSRAM) is written to the SD card by otaLoop() on the main loop, one slice per
// pass (the card shares its SPI bus with the LCD and the radio: written from the download task the file came out 32
// bytes short). It is then read back and hashed, and renamed from .part only once verified, so the Launcher never
// lists a damaged file. "busy" stays set until the save ends.
struct SdSave {uint8_t *ram=nullptr;uint32_t size=0,at=0;bool verifying=false;String name,expected;File file;mbedtls_sha256_context hash;};
static SdSave sdSave;static volatile bool sdSavePending=false;
constexpr uint32_t SdSlice=32*1024;
static void sdSaveFinish(bool ok,String problem){
 String part=sdSave.name+".part";if(sdSave.file)sdSave.file.close();
 if(sdSave.verifying){uint8_t digest[32];mbedtls_sha256_finish(&sdSave.hash,digest);mbedtls_sha256_free(&sdSave.hash);
  if(ok&&sdSave.expected!=mm::hexString(digest,32).c_str()){ok=false;problem="SD card copy damaged / discarded";}}
 // A failed rename keeps the verified .part (the Launcher does not list it): the old copy is already gone.
 if(ok){SD.remove(sdSave.name);ok=SD.rename(part,sdSave.name);if(!ok)problem="SD card rename failed";}else SD.remove(part);
 heap_caps_free(sdSave.ram);sdSave.ram=nullptr;sdSavePending=false;
 if(ok)Serial.printf("OTA_SAVED file=%s bytes=%lu\n",sdSave.name.c_str(),(unsigned long)sdSave.size);else Serial.printf("OTA_FAILED %s\n",problem.c_str());
 xSemaphoreTake(mutex,portMAX_DELAY);if(ok){state.phase=OtaPhase::Saved;state.sdFile=sdSave.name;state.message="Saved "+sdSave.name.substring(1);}else{state.phase=OtaPhase::Error;state.message=problem;}
 busy=false;publish();xSemaphoreGive(mutex);
}
static void sdSaveStep(){
 if(!sdSavePending)return;String part=sdSave.name+".part";uint32_t n=std::min(SdSlice,sdSave.size-sdSave.at);
 if(!sdSave.verifying){
  if(!sdSave.at){SD.remove(part);sdSave.file=SD.open(part,FILE_WRITE);if(!sdSave.file){sdSaveFinish(false,"SD card unavailable");return;}}
  if(sdSave.file.write(sdSave.ram+sdSave.at,n)!=n){sdSaveFinish(false,"SD card write failed");return;}
  sdSave.at+=n;
  if(sdSave.at==sdSave.size){
   sdSave.file.flush();sdSave.file.close();sdSave.file=SD.open(part,FILE_READ);
   if(!sdSave.file||sdSave.file.size()!=sdSave.size){sdSaveFinish(false,"SD card copy damaged / discarded");return;}
   sdSave.verifying=true;sdSave.at=0;mbedtls_sha256_init(&sdSave.hash);mbedtls_sha256_starts(&sdSave.hash,0);
  }
 }else{
  // Read back into the image's own buffer: the RAM copy is no longer needed and the hash covers what the card returns.
  if(sdSave.file.read(sdSave.ram+sdSave.at,n)!=n){sdSaveFinish(false,"SD card copy damaged / discarded");return;}
  mbedtls_sha256_update(&sdSave.hash,sdSave.ram+sdSave.at,n);sdSave.at+=n;
  if(sdSave.at==sdSave.size){sdSaveFinish(true,"");return;}
 }
 xSemaphoreTake(mutex,portMAX_DELAY);state.done=(sdSave.verifying?sdSave.size:0)+sdSave.at;publish();xSemaphoreGive(mutex);
}

static void installWorker(void*){
 xSemaphoreTake(mutex,portMAX_DELAY);String url=state.server+path,version=state.version,expected=sha256,edition=offerEdition;uint32_t size=state.size;int32_t code=state.code;bool toSd=state.toSd;state.phase=OtaPhase::Downloading;state.done=0;state.message="Downloading "+version;publish();xSemaphoreGive(mutex);
 if(toSd){
  // A name of its own per edition and version; the Launcher shows the first 20 characters as the app name.
  String name="/MatterMesh-"+String(edition=="full"?"Full":"Free")+"-"+version+".bin",problem;
  uint8_t *ram=(uint8_t*)heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!ram)problem="Not enough memory for the image";
  else if(download(url,size,expected,ram,problem)){
   sdSave.ram=ram;sdSave.size=size;sdSave.at=0;sdSave.verifying=false;sdSave.name=name;sdSave.expected=expected;
   xSemaphoreTake(mutex,portMAX_DELAY);state.phase=OtaPhase::Verifying;state.done=0;state.message="Saving to the SD card";publish();xSemaphoreGive(mutex);
   sdSavePending=true;networkJobEnd();vTaskDelete(nullptr);return;
  }
  if(ram)heap_caps_free(ram);
  Serial.printf("OTA_FAILED %s\n",problem.c_str());setPhase(OtaPhase::Error,problem);
  xSemaphoreTake(mutex,portMAX_DELAY);busy=false;xSemaphoreGive(mutex);networkJobEnd();vTaskDelete(nullptr);return;
 }
 String problem;bool ok=download(url,size,expected,nullptr,problem);
 if(ok){
  setPhase(OtaPhase::Restarting,"Installed "+version+" / restarting");Serial.printf("OTA_INSTALLED version=%s code=%ld\n",version.c_str(),long(code));
  // otaLoop() restarts in 2.5 s from the main task, between its own SD writes, after closing the card (the SD is
  // written by the main task; the network workers only fetch). busy stays set.
  xSemaphoreTake(mutex,portMAX_DELAY);restartAt=millis()+2500;if(!restartAt)restartAt=1;xSemaphoreGive(mutex);networkJobEnd();vTaskDelete(nullptr);
 }
 Serial.printf("OTA_FAILED %s\n",problem.c_str());setPhase(OtaPhase::Error,problem);
 xSemaphoreTake(mutex,portMAX_DELAY);busy=false;xSemaphoreGive(mutex);networkJobEnd();vTaskDelete(nullptr);
}

static bool spawn(TaskFunction_t worker,const char *name,uint32_t stack){
 xSemaphoreTake(mutex,portMAX_DELAY);if(busy){xSemaphoreGive(mutex);return false;}busy=true;xSemaphoreGive(mutex);
 networkJobBegin();if(xTaskCreatePinnedToCore(worker,name,stack,nullptr,1,nullptr,0)==pdPASS)return true;networkJobEnd();
 xSemaphoreTake(mutex,portMAX_DELAY);busy=false;xSemaphoreGive(mutex);setPhase(OtaPhase::Error,"Update worker unavailable");return false;
}

bool otaLauncherHosted(){return launcherHosted;}
void otaBegin(){
 mutex=xSemaphoreCreateMutex();preferences.begin("mmota",false);
 // bmorcelli's Launcher lives in an app partition of subtype "test" (its bootloader starts it at power-on).
 launcherHosted=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_TEST,nullptr)!=nullptr;
 license=preferences.getString("license","");if(!licenseValid(license))license="";
 servers=preferences.isKey("servers")?preferences.getString("servers",DefaultServers):String(DefaultServers);
 // A list saved from serial keeps the servers of this build in front: a T-Deck set by hand to the NAS still moves to new defaults.
 {String missing,rest=String(DefaultServers)+" ";int start=0;
  while(start<int(rest.length())){int end=rest.indexOf(' ',start);String url=rest.substring(start,end);start=end+1;
   if(url.length()&&(" "+servers+" ").indexOf(" "+url+" ")<0)missing+=(missing.length()?" ":"")+url;}
  if(missing.length())servers=missing+(servers.length()?" "+servers:"");}
 int32_t tried=preferences.getInt("try",0),from=preferences.getInt("from",0);
 if(tried){
  // The bootloader booted the old slot again: the new image crashed before it was marked healthy.
  // "try" stays until otaLoop() marks the new image healthy, so a crash before that is still seen here after the rollback.
  // Same build number after a Free -> Full switch: the edition tells whether the new image is the one running.
  String edition=preferences.getString("tryed","");
  if(tried==MM_VERSION_CODE&&(!edition.length()||edition==Edition)){state.installed=true;pendingReport="ok";pendingDetail="installed from "+String(from);}
  else{state.rolledBack=true;pendingReport="rollback";pendingDetail="build "+String(tried)+(edition.length()?" "+edition:"")+" rolled back";state.message="Update rolled back";
   String badEdition=edition.length()?edition:String(Edition);
   String stored=preferences.getString("bade","");// none: saved by an older build, same count
   int32_t count=preferences.getInt("bad",0)==tried&&(!stored.length()||stored==badEdition)?preferences.getInt("badn",0)+1:1;preferences.putInt("bad",tried);preferences.putInt("badn",count);preferences.putString("bade",badEdition);
   preferences.remove("try");preferences.remove("from");preferences.remove("tryed");}
 }
 rejected=preferences.getInt("badn",0)>=2?preferences.getInt("bad",0):0;rejectedEdition=preferences.getString("bade","");state.refused=rejected;// offered again after one rollback
 const esp_partition_t *running=esp_ota_get_running_partition();esp_ota_img_states_t image;
 Serial.printf("OTA_BOOT board=%s code=%ld slot=%s state=%d tried=%ld launcher=%d\n",otaBoard(),long(MM_VERSION_CODE),running?running->label:"?",running&&esp_ota_get_state_partition(running,&image)==ESP_OK?int(image):-1,long(tried),launcherHosted);
}

void otaLoop(bool wifiConnected,bool uiReady){
 sdSaveStep();
 {xSemaphoreTake(mutex,portMAX_DELAY);uint32_t at=restartAt;xSemaphoreGive(mutex);if(at&&int32_t(millis()-at)>=0){SD.end();ESP.restart();}}
 if(!healthy&&uiReady&&millis()>=OTA_HEALTHY_AFTER_MS){
  healthy=true;const esp_partition_t *running=esp_ota_get_running_partition();esp_ota_img_states_t image;
  if(running&&esp_ota_get_state_partition(running,&image)==ESP_OK&&image==ESP_OTA_IMG_PENDING_VERIFY){esp_ota_mark_app_valid_cancel_rollback();Serial.println("OTA_VALID image marked healthy");}
  if(preferences.getInt("try",0)==MM_VERSION_CODE){preferences.remove("try");preferences.remove("from");preferences.remove("tryed");}
  if(preferences.getInt("bad",0)==MM_VERSION_CODE&&preferences.getString("bade",Edition)==Edition){preferences.remove("bad");preferences.remove("badn");preferences.remove("bade");}// this build runs: its old rollback count goes
 }
 if(!wifiConnected){connectedAt=0;return;}
 if(!connectedAt)connectedAt=millis();
 bool due=!checkedOnce?uint32_t(millis()-connectedAt)>=OTA_FIRST_CHECK_MS:uint32_t(millis()-lastCheck)>=OTA_CHECK_EVERY_MS;
 // The first check waits for the image to be healthy, so its report can say "ok".
 if(due&&healthy)otaCheck();// otaCheck() does nothing while a check or an install runs
}

bool otaCheck(){
 {xSemaphoreTake(mutex,portMAX_DELAY);bool running=busy;xSemaphoreGive(mutex);if(running)return false;}// a check or an install is running: its phase stays
 if(WiFi.status()!=WL_CONNECTED){setPhase(OtaPhase::Error,"Wi-Fi not connected");return false;}
 if(!otaServerCount()){checkedOnce=true;lastCheck=millis();setPhase(OtaPhase::Error,"No update server configured");return false;}
 checkedOnce=true;lastCheck=millis();return spawn(checkWorker,"matter-ota-check",8192);
}
bool otaInstall(){
 xSemaphoreTake(mutex,portMAX_DELAY);bool ready=state.phase==OtaPhase::Available,toSd=state.toSd;uint32_t size=state.size;xSemaphoreGive(mutex);
 if(!ready||WiFi.status()!=WL_CONNECTED)return false;
 // Under the Launcher the card is checked here, on the main loop, before anything is downloaded: room for the .part next
 // to an older copy of the same file (replaced only once the new one is verified) + 64 KB slack.
 if(toSd){uint64_t total=SD.totalBytes(),used=total?SD.usedBytes():0;
  if(!total||total-used<uint64_t(size)+65536){setPhase(OtaPhase::Error,total?"Not enough space on the SD card":"SD card unavailable");return false;}}
 return spawn(installWorker,"matter-ota-install",12288);
}
OtaState otaState(){xSemaphoreTake(mutex,portMAX_DELAY);auto view=state;xSemaphoreGive(mutex);return view;}
uint32_t otaRevision(){return state.revision;}
int otaServerCount(){xSemaphoreTake(mutex,portMAX_DELAY);int n=servers.length()?1:0;for(unsigned i=0;i<servers.length();i++)n+=servers[i]==' ';xSemaphoreGive(mutex);return n;}
void otaSetServers(const String &list){
 // Only http(s) base URLs: anything else would never answer and fails the whole check.
 String clean,rest=list+" ";int start=0;
 while(start<int(rest.length())){int end=rest.indexOf(' ',start);String url=rest.substring(start,end);start=end+1;if(url.startsWith("http://")||url.startsWith("https://"))clean+=(clean.length()?" ":"")+url;}
 // An empty list goes back to the build default and forgets the saved one, so a later build's default applies.
 if(!clean.length()){xSemaphoreTake(mutex,portMAX_DELAY);servers=DefaultServers;xSemaphoreGive(mutex);if(preferences.isKey("servers"))preferences.remove("servers");return;}
 xSemaphoreTake(mutex,portMAX_DELAY);servers=clean;xSemaphoreGive(mutex);if(preferences.getString("servers","")!=clean)preferences.putString("servers",clean);
}
}
