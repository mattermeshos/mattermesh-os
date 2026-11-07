#pragma once
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "weather_ca.h"
namespace device {
// One HTTPS setup for the open-data services (weather, geolocation, storms, radar, map tiles): the roots of
// weather_ca.h, 8 s for the TLS handshake and for each read, 6 s to connect (a TCP connect over the Plus Wi-Fi has
// taken 3 s; 5 s with a 6 s handshake made weather and storm checks fail where radar and tiles went through).
inline void secureClient(WiFiClientSecure &client,HTTPClient &http){
 client.setCACert(WEATHER_ROOT_CA);client.setHandshakeTimeout(8);client.setTimeout(8);http.setConnectTimeout(6000);http.setTimeout(8000);
}
}
