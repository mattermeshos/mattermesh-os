#pragma once
#include <Preferences.h>
// One-shot NVS access: open the namespace, read or write one key, close. Writes report whether the value was stored.
namespace device {
inline int prefGetInt(const char *ns,const char *key,int fallback){Preferences p;if(!p.begin(ns,true))return fallback;int v=p.getInt(key,fallback);p.end();return v;}
inline bool prefPutInt(const char *ns,const char *key,int value){Preferences p;if(!p.begin(ns,false))return false;bool ok=p.putInt(key,value)==sizeof(int32_t);p.end();return ok;}
inline uint32_t prefGetUInt(const char *ns,const char *key,uint32_t fallback){Preferences p;if(!p.begin(ns,true))return fallback;uint32_t v=p.getUInt(key,fallback);p.end();return v;}
inline bool prefPutUInt(const char *ns,const char *key,uint32_t value){Preferences p;if(!p.begin(ns,false))return false;bool ok=p.putUInt(key,value)==sizeof(value);p.end();return ok;}
// A missing key reads as "" (without the NVS "not found" log).
inline String prefGetString(const char *ns,const char *key){Preferences p;if(!p.begin(ns,true))return String();String v=p.isKey(key)?p.getString(key,""):String();p.end();return v;}
// An empty value removes the key.
inline bool prefPutString(const char *ns,const char *key,const String &value){Preferences p;if(!p.begin(ns,false))return false;bool ok=value.length()?p.putString(key,value)==value.length():(!p.isKey(key)||p.remove(key));p.end();return ok;}
}
