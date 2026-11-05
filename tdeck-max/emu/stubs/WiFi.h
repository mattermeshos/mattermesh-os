#pragma once
// Emulator Wi-Fi: one virtual access point (default "EMU-WiFi", password "meshcore", set with --wifi SSID:PASS)
// that scans, accepts or refuses the credentials like a real AP and gives the PC's LAN address; TCP clients and
// servers are real host sockets (the MeshCore app reaches the companion server, port 5000 + --port-offset).
// HTTPS: WiFiClientSecure on OpenSSL (emu/devices/tls_emu.cpp) + the real Arduino-ESP32 HTTPClient.
// Implementation: emu/devices/wifi_emu.cpp.
#include <Arduino.h>
#include <IPAddress.h>
#include <functional>
#include <memory>
typedef enum {WIFI_OFF=0,WIFI_STA=1,WIFI_AP=2,WIFI_AP_STA=3} wifi_mode_t;
typedef enum {WL_NO_SHIELD=255,WL_IDLE_STATUS=0,WL_NO_SSID_AVAIL=1,WL_SCAN_COMPLETED=2,WL_CONNECTED=3,WL_CONNECT_FAILED=4,WL_CONNECTION_LOST=5,WL_DISCONNECTED=6} wl_status_t;
typedef enum {WIFI_AUTH_OPEN=0,WIFI_AUTH_WEP,WIFI_AUTH_WPA_PSK,WIFI_AUTH_WPA2_PSK,WIFI_AUTH_WPA_WPA2_PSK,WIFI_AUTH_WPA2_ENTERPRISE,WIFI_AUTH_WPA3_PSK,WIFI_AUTH_WPA2_WPA3_PSK} wifi_auth_mode_t;
enum {WIFI_REASON_AUTH_FAIL=202,WIFI_REASON_NO_AP_FOUND=201,WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT=15,WIFI_REASON_HANDSHAKE_TIMEOUT=204,WIFI_REASON_ASSOC_LEAVE=8};
constexpr int16_t WIFI_SCAN_RUNNING=-1,WIFI_SCAN_FAILED=-2;
typedef enum {ARDUINO_EVENT_WIFI_STA_CONNECTED=4,ARDUINO_EVENT_WIFI_STA_DISCONNECTED=5,ARDUINO_EVENT_WIFI_STA_GOT_IP=7,ARDUINO_EVENT_WIFI_STA_LOST_IP=8} arduino_event_id_t;
struct wifi_event_sta_disconnected_t {uint8_t ssid[32];uint8_t ssid_len;uint8_t bssid[6];uint8_t reason;};
typedef union {wifi_event_sta_disconnected_t wifi_sta_disconnected;} arduino_event_info_t;
typedef std::function<void(arduino_event_id_t,arduino_event_info_t)> WiFiEventFuncCb;

// Emulator configuration (main_emu.cpp): the virtual AP and the offset added to every listening port.
namespace emuwifi {void configure(const char *ssid,const char *password,int portOffset);}

// Scan timeout lives in a protected static of WiFiScanClass (connectivity.cpp sets it through a derived class).
class WiFiScanClass {protected:static inline uint32_t _scanTimeout=10000;};

// TCP client on a host socket; copies share the connection (as Arduino-ESP32's WiFiClient).
class WiFiClient:public Stream {
public:
 struct Socket;
 WiFiClient();
 explicit WiFiClient(std::shared_ptr<Socket> s);
 virtual ~WiFiClient();
 virtual int connect(const char *host,uint16_t port);
 virtual int connect(const char *host,uint16_t port,int32_t timeoutMs);
 virtual int connect(IPAddress ip,uint16_t port);
 virtual uint8_t connected();
 virtual void stop();
 void setNoDelay(bool on);
 IPAddress remoteIP() const;// peer address (ESP32 WiFiClient::remoteIP)
 int fd() const;// socket descriptor, -1 when not connected (ESP32 WiFiClient::fd)
 // ESP32 semantics: seconds (sets the Stream timeout used by readBytes / readStringUntil to seconds*1000).
 int setTimeout(uint32_t seconds){Stream::setTimeout(seconds*1000);return 0;}
 int available()override;
 int read()override;
 virtual int read(uint8_t *buf,size_t n);
 int peek()override;
 size_t write(uint8_t b)override;
 size_t write(const uint8_t *buf,size_t n)override;
 void flush()override{}
 explicit operator bool(){return connected();}
 bool operator==(const WiFiClient &o) const {return sock==o.sock;}
protected:
 std::shared_ptr<Socket> sock;
};
class WiFiServer {
public:
 explicit WiFiServer(uint16_t port=80);
 ~WiFiServer();
 void begin();
 void end();
 void setNoDelay(bool on){noDelay=on;}
 WiFiClient available();
 WiFiClient accept(){return available();}
 uint16_t port;
private:
 int fd=-1;bool noDelay=false;
};
class WiFiClass {
public:
 bool mode(wifi_mode_t m);
 wifi_mode_t getMode();
 wl_status_t begin(const char *ssid,const char *password=nullptr);
 bool disconnect(bool wifiOff=false,bool eraseAp=false);
 wl_status_t status();
 void setAutoReconnect(bool){}
 bool setSleep(bool s){sleep=s;return true;}
 bool getSleep(){return sleep;}
 void persistent(bool){}
 int onEvent(WiFiEventFuncCb cb);
 int16_t scanNetworks(bool async=false,bool showHidden=false);
 int16_t scanComplete();
 void scanDelete();
 String SSID();String SSID(uint8_t i);String psk();
 int32_t RSSI();int32_t RSSI(uint8_t i);
 wifi_auth_mode_t encryptionType(uint8_t i);
 uint8_t *BSSID(uint8_t i=0);
 IPAddress localIP();
 uint8_t *macAddress(uint8_t *mac);
 String macAddress();
private:
 bool sleep=false;
};
extern WiFiClass WiFi;
