#pragma once
// Emulator TLS client on OpenSSL with the Arduino-ESP32 2.x API. Like the device's mbedTLS client, a server
// certificate is accepted only if it chains to a CA given with setCACert (or with setInsecure); the host name is
// checked and SNI is sent. Implementation: emu/devices/tls_emu.cpp.
#include "WiFi.h"
class WiFiClientSecure:public WiFiClient {
public:
 struct Tls;
 WiFiClientSecure();
 ~WiFiClientSecure()override;
 void setCACert(const char *rootCA){ca=rootCA;}
 void setCertificate(const char*){}
 void setPrivateKey(const char*){}
 void setInsecure(){insecure=true;}
 // Root bundle (esp_crt_bundle on the device): the emulator verifies against the host system store, which is
 // where tools/make_ca_bundle.py takes the device bundle from.
 void setCACertBundle(const uint8_t*){bundle=true;}
 void setHandshakeTimeout(unsigned long seconds){handshakeSeconds=seconds;}
 int connect(const char *host,uint16_t port)override;
 int connect(const char *host,uint16_t port,int32_t timeoutMs)override;
 int connect(IPAddress ip,uint16_t port)override;
 uint8_t connected()override;
 void stop()override;
 int available()override;
 int read()override;
 int read(uint8_t *buf,size_t n)override;
 int peek()override;
 size_t write(uint8_t b)override;
 size_t write(const uint8_t *buf,size_t n)override;
 int lastError(char *buf,size_t size);
private:
 const char *ca=nullptr;bool insecure=false,bundle=false;unsigned long handshakeSeconds=120;std::shared_ptr<Tls> tls;
};
