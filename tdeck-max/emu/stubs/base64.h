#pragma once
// Arduino-ESP32 core base64 (HTTPClient basic auth).
#include <Arduino.h>
class base64 {
public:
 static String encode(const uint8_t *data,size_t length){static const char t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";String out;
  for(size_t i=0;i<length;i+=3){uint32_t v=uint32_t(data[i])<<16|(i+1<length?uint32_t(data[i+1])<<8:0)|(i+2<length?data[i+2]:0);
   out+=t[v>>18&63];out+=t[v>>12&63];out+=i+1<length?t[v>>6&63]:'=';out+=i+2<length?t[v&63]:'=';}return out;}
 static String encode(const String &text){return encode(reinterpret_cast<const uint8_t*>(text.c_str()),text.length());}
};
