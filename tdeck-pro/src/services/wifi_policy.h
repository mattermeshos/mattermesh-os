#pragma once
#include <string>
#include <stdint.h>
namespace device {
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS=20000,WIFI_RETRY_MS=30000;
// Arduino disconnect is asynchronous: never call begin against the old link.
struct WifiConnectionStart {
 bool pending=false;uint32_t queuedAt=0;
 void queue(uint32_t now){pending=true;queuedAt=now;}
 void cancel(){pending=false;}
 bool ready(uint32_t now,bool driverConnected)const{return pending&&!driverConnected&&uint32_t(now-queuedAt)>=150;}
 bool timedOut(uint32_t now)const{return pending&&uint32_t(now-queuedAt)>=2000;}
};
inline bool validWifiCredentials(const std::string &ssid,const std::string &password){
 return !ssid.empty()&&ssid.size()<=32&&(password.empty()||(password.size()>=8&&password.size()<=63))&&ssid.find('\0')==std::string::npos&&password.find('\0')==std::string::npos;
}
inline bool wifiMayCommit(bool staged,bool freshIP,const std::string &expected,const std::string &actual){return !staged||(freshIP&&expected==actual);}
inline std::string wifiInputDisplay(const std::string &password,bool visible){return visible?password:std::string(password.size(),'*');}
struct WifiRetryPolicy {bool allowed=true;void disconnect(){allowed=false;}void connect(){allowed=true;}bool due(bool enabled,bool connected,bool connecting,bool scanning,uint32_t now,uint32_t since)const{return allowed&&enabled&&!connected&&!connecting&&!scanning&&uint32_t(now-since)>=WIFI_RETRY_MS;}};
}
