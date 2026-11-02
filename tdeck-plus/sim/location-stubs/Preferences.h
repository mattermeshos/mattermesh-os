#pragma once
#include <map>
#include <string>
#include <stddef.h>
inline std::map<std::string,double> fakePrefs;
inline bool fakePrefsFailure=false;
class Preferences {
public:
 bool begin(const char*,bool){return true;}
 bool getBool(const char *key,bool fallback=false){return fakePrefs.count(key)?fakePrefs[key]!=0:fallback;}
 double getDouble(const char *key,double fallback=0){return fakePrefs.count(key)?fakePrefs[key]:fallback;}
 size_t putBool(const char *key,bool value){if(fakePrefsFailure)return 0;fakePrefs[key]=value;return 1;}
 size_t putDouble(const char *key,double value){fakePrefs[key]=value;return sizeof(double);}
 int getInt(const char *key,int fallback=0){return fakePrefs.count(key)?int(fakePrefs[key]):fallback;}
 size_t putInt(const char *key,int value){if(fakePrefsFailure)return 0;fakePrefs[key]=value;return sizeof(int);}
 void end(){}
};
