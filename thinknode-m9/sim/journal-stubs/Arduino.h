#pragma once
#include <string>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <type_traits>
#define HEX 16
#define FILE_APPEND 1
class String:public std::string {
public:
 using std::string::string;String(const std::string &v):std::string(v){}
 template<typename T,typename std::enable_if<std::is_integral<T>::value,int>::type=0>String(T v):std::string(std::to_string(v)){}
 String(uint32_t v,int base){std::ostringstream stream;if(base==16)stream<<std::hex;stream<<v;assign(stream.str());}
 unsigned length()const{return size();}int indexOf(char c,int at=0)const{auto n=find(c,at);return n==npos?-1:int(n);}
 String substring(int start,int end=-1)const{return substr(start,end<0?npos:size_t(end-start));}
 bool startsWith(const char *prefix)const{return rfind(prefix,0)==0;}int toInt()const{return std::atoi(c_str());}
};
