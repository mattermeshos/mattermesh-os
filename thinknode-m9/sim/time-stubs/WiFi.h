#pragma once
constexpr int WL_CONNECTED=3;
constexpr int WIFI_OFF=0,WIFI_STA=1;
inline int fakeWiFiStatus=0,fakeWiFiMode=WIFI_OFF;
struct FakeWiFi {int status(){return fakeWiFiStatus;}int getMode(){return fakeWiFiMode;}};
inline FakeWiFi WiFi;
