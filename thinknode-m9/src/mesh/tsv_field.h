#pragma once
#include <Arduino.h>
// Field `index` of a tab-separated SD row (journal and mesh service rows); empty when the row has fewer fields.
inline String field(const String &s,int index){int a=0;while(index--){a=s.indexOf('\t',a);if(a<0)return "";a++;}int b=s.indexOf('\t',a);return b<0?s.substring(a):s.substring(a,b);}
