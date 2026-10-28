// MeshCore companion app links (included by main.cpp): Bluetooth (NimBLE UART service, passkey), USB cable
// (frames '<' len16 payload in, '>' len16 payload out, next to the text commands of the serial console) and Wi-Fi
// (TCP server on port 5000, same framing, only with Settings > Bluetooth > App over Wi-Fi on). The protocol itself is
// matter::companionFrame (src/mesh/companion.inc).
#include <WiFi.h>
static matter::CompanionSession appBle,appUsb,appTcp;
static WiFiServer appServer(5000);static WiFiClient appClient;static bool appServerOn=false,appWifiEnabled=false;
static uint32_t appUsbAt=0,appTcpAt=0,appBleAt=0;static uint32_t appPinValue=0;
// Bluetooth name chosen in Settings > Bluetooth (the part after "MeshCore-"; empty = node name). NVS mmble/name.
static String appBleCustomName;
static void appBleWrite(void*,const uint8_t *f,size_t n){device::bleCompanionSend(f,n);}
static void appUsbWrite(void*,const uint8_t *f,size_t n){uint8_t h[3]={'>',uint8_t(n&0xff),uint8_t(n>>8)};Serial.write(h,3);Serial.write(f,n);}
static void appTcpWrite(void*,const uint8_t *f,size_t n){if(!appClient||!appClient.connected())return;uint8_t buf[3+matter::COMPANION_FRAME_MAX];buf[0]='>';buf[1]=uint8_t(n&0xff);buf[2]=uint8_t(n>>8);memcpy(buf+3,f,n);appClient.write(buf,3+n);}
bool appUsbLinked(){return appUsbAt&&uint32_t(millis()-appUsbAt)<60000;}// a USB app talked within the last minute
static uint32_t appPin(){return appPinValue;}
static bool appSetPin(uint32_t pin){if(!pin)pin=100000+esp_random()%900000;if(!device::prefPutInt("mmble","pin",int(pin)))return false;appPinValue=pin;device::bleCompanionConfig(matter::snapshot().config.name,pin);return true;}
// Saves and applies the Bluetooth name; empty goes back to the node name. False: invalid or not saved.
static bool appSetBleName(String name){name.trim();if(name.length()>device::BLE_NAME_MAX_BYTES)return false;for(unsigned i=0;i<name.length();i++)if(name[i]<32||name[i]>126)return false;
 if(!device::prefPutString("mmble","name",name))return false;
 appBleCustomName=name;device::bleCompanionCustomName(name);return true;}
static void appLocation(int32_t &lat,int32_t &lon){auto g=device::locationState();if(g.configured){lat=int32_t(lround(g.latitude*1e6));lon=int32_t(lround(g.longitude*1e6));}}
// App CMD_SET_CHANNEL. Our channel slots follow the conversation list (direct chats take slots too), so the app's
// index edits a channel conversation in place, anything else appends one (scope "*") like Messages > New channel.
// A channel already known by name or key is left as it is; deleting (empty name or zero key) is not supported.
static uint8_t appSetChannel(uint8_t idx,const char *rawName,const uint8_t secret[16]){
 String key(mm::hexString(secret,16).c_str());
 String name=String(mm::cleanChannelName(rawName).c_str());
 if(!name.length()||mm::zeroSecret(key.c_str()))return 1;// ERR_UNSUPPORTED
 if(!sdReady)return 4;// ERR_BAD_STATE
 String reason;
 if(idx<chatCount&&!chats[idx].direct){
  String oldName=chats[idx].name,oldKey=channelKeys[idx];chats[idx].name=name;channelKeys[idx]=key;
  if(!saveBindings()||!saveChats()){chats[idx].name=oldName;channelKeys[idx]=oldKey;saveBindings();saveChats();return 4;}
  channelReady[idx]=matter::channel(idx,name,key,chats[idx].scope,reason);
  Serial.printf("CHANNEL_SAVED id=%d form=app ready=%d\n",idx,channelReady[idx]);render();return 0;}
 for(int i=0;i<chatCount;i++)if(!chats[i].direct&&(chats[i].name==name||channelKeys[i]==key))return 0;
 // Full: the app's "Import configuration" stops at the first error and drops everything after it (contacts too),
 // so a channel that does not fit is skipped with OK and counted on the screen instead.
 if(chatCount>=MAX_CHATS){static uint32_t skippedAt=0;static int skipped=0;if(uint32_t(millis()-skippedAt)>60000)skipped=0;skippedAt=millis();skipped++;
  notice=String("Channel list full (")+MAX_CHATS+"): "+skipped+" from the app not added";Serial.printf("CHANNEL_SKIPPED full=%d skipped=%d\n",MAX_CHATS,skipped);render();return 0;}
 int id=chatCount;chats[id]={name,"*",false};channelKeys[id]=key;peerKeys[id]="";unreadMessages[id]=0;chatDrafts.saved[id]="";mutedChannels&=~(uint32_t(1)<<id);chatCount++;
 if(!saveBindings()){chatCount--;channelKeys[id]="";return 4;}
 if(!saveChats()){chatCount--;channelKeys[id]="";saveBindings();return 4;}
 channelReady[id]=matter::channel(id,name,key,"*",reason);
 Serial.printf("CHANNEL_SAVED id=%d form=app ready=%d\n",id,channelReady[id]);render();return 0;
}
void appLinkBegin(){
 appPinValue=uint32_t(device::prefGetInt("mmble","pin",0));if(appPinValue<100000||appPinValue>999999){appPinValue=100000+esp_random()%900000;device::prefPutInt("mmble","pin",int(appPinValue));}
 appWifiEnabled=device::prefGetInt("mmble","tcp",0)!=0;
 appBleCustomName=device::prefGetString("mmble","name");device::bleCompanionCustomName(appBleCustomName);
 matter::CompanionHooks h;h.batteryMv=[]()->uint16_t{return uint16_t(device::batteryState().voltage);};
 h.storageUsedKb=[]()->uint32_t{return sdReady?uint32_t(SD.usedBytes()/1024):0;};h.storageTotalKb=[]()->uint32_t{return sdReady?uint32_t(SD.totalBytes()/1024):0;};
 h.reboot=[](){Serial.flush();delay(300);ESP.restart();};h.pin=appPin;h.setPin=appSetPin;h.location=appLocation;h.setChannel=appSetChannel;matter::companionHooks(h);
 device::bleCompanionConfig(matter::snapshot().config.name,appPinValue);// before connectivityBegin(): the BLE name comes from the node name
}
void appWifiToggle(){appWifiEnabled=!appWifiEnabled;device::prefPutInt("mmble","tcp",appWifiEnabled?1:0);if(!appWifiEnabled&&appServerOn){if(appClient)appClient.stop();appServer.end();appServerOn=false;}}
// USB: called by the serial reader for the bytes after a '<' at the start of a line.
static uint8_t appUsbBuf[matter::COMPANION_FRAME_MAX];static int appUsbState=0;static uint16_t appUsbLen=0,appUsbGot=0;
bool appUsbByte(uint8_t c){// true while a binary frame is being assembled (the byte is consumed)
 if(appUsbState==0){if(c!='<')return false;appUsbState=1;return true;}
 if(appUsbState==1){appUsbLen=c;appUsbState=2;return true;}
 if(appUsbState==2){appUsbLen|=uint16_t(c)<<8;appUsbGot=0;appUsbState=appUsbLen&&appUsbLen<=matter::COMPANION_FRAME_MAX?3:0;return true;}
 appUsbBuf[appUsbGot++]=c;if(appUsbGot>=appUsbLen){appUsbState=0;appUsbAt=millis();matter::companionFrame(appUsb,appUsbBuf,appUsbLen,appUsbWrite,nullptr);}
 return true;
}
// Room in the socket's send buffer now (select with a zero timeout): WiFiClient::write waits up to 10 x 1 s when the
// phone stops reading, and availableForWrite() is not implemented in this core. Nothing is read or pumped without it.
static bool appTcpWritable(){int fd=appClient.fd();if(fd<0)return false;fd_set w;FD_ZERO(&w);FD_SET(fd,&w);timeval tv{0,0};return select(fd+1,nullptr,&w,nullptr,&tv)>0;}
static void appTcpLoop(){
 static uint8_t frame[matter::COMPANION_FRAME_MAX];static uint16_t frameLen=0;static size_t got=0;static uint32_t frameAt=0;// partial frame (see below)
 bool want=appWifiEnabled&&device::wifiConnected();
 if(want&&!appServerOn){appServer.begin();appServer.setNoDelay(true);appServerOn=true;}
 if(!want){if(appServerOn){if(appClient)appClient.stop();appServer.end();appServerOn=false;}return;}
 if(!appClient||!appClient.connected()){WiFiClient next=appServer.available();if(next){appClient=next;appClient.setNoDelay(true);appTcp=matter::CompanionSession();frameLen=0;}}
 if(!appClient||!appClient.connected())return;
 // Frames: '<' + length (LE) + payload; anything else is skipped byte by byte. A payload is read as it arrives,
 // across passes (the UI loop never waits on the socket); one with no new byte for 2 s drops the connection.
 if(!appTcpWritable())return;// the answer could not be sent now: leave the request in the socket
 if(!frameLen&&appClient.available()>=3){int t=appClient.peek();if(t!='<'){appClient.read();return;}
  uint8_t h[3];appClient.read(h,3);uint16_t n=h[1]|(uint16_t(h[2])<<8);if(!n||n>sizeof(frame)){appClient.stop();return;}// out of sync: the rest would read as frames
  frameLen=n;got=0;frameAt=millis();}
 if(frameLen){int k=appClient.available()>0?appClient.read(frame+got,frameLen-got):0;if(k>0){got+=k;frameAt=millis();}
  if(got==frameLen){appTcpAt=millis();uint16_t n=frameLen;frameLen=0;matter::companionFrame(appTcp,frame,n,appTcpWrite,nullptr);}
  else if(millis()-frameAt>2000){frameLen=0;appClient.stop();}}// stalled for 2 s: drop the link, the app reconnects
 for(int k=0;k<16&&appTcpWritable()&&matter::companionPump(appTcp,appTcpWrite,nullptr);k++){}
}
void appLinkLoop(){
 uint8_t frame[matter::COMPANION_FRAME_MAX];size_t n=0;
 for(int k=0;k<4&&device::bleCompanionTake(frame,n);k++){appBleAt=millis();matter::companionFrame(appBle,frame,n,appBleWrite,nullptr);}
 // Several frames per loop (one each made a 1000-contact listing take ~100 s); BLE stays at a few notifications
 // so NimBLE's buffers never run out.
 if(device::bleCompanionConnected()){for(int k=0;k<3&&matter::companionPump(appBle,appBleWrite,nullptr);k++){}}else{appBle.listing=false;appBle.active=false;}
 if(appUsbLinked()){for(int k=0;k<8&&matter::companionPump(appUsb,appUsbWrite,nullptr);k++){}}else{appUsb.listing=false;appUsb.active=false;}
 appTcpLoop();
}
void appLinkFill(mm::State &v){
 v.appBleName=device::bleCompanionName().c_str();v.appBleEditing=bleNameEditing;v.appBleInput=bleNameInput.c_str();v.appPin=std::to_string(appPinValue);v.appWifi=appWifiEnabled;v.appBleLinked=device::bleCompanionConnected();
 v.appUsbLinked=appUsbLinked();v.appWifiLinked=appClient&&appClient.connected();
 v.appWifiDetail=device::wifiConnected()?std::string(WiFi.localIP().toString().c_str())+":5000":"Waiting for Wi-Fi";
}
