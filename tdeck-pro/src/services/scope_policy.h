#pragma once
#include <string>
namespace device {
constexpr int MAX_REGION_SCOPES=64;
inline std::string normalizedScope(std::string value){
 auto first=value.find_first_not_of(" \r\n\t"),last=value.find_last_not_of(" \r\n\t");
 if(first==std::string::npos)return "";value=value.substr(first,last-first+1);
 if(value[0]=='#')value.erase(0,1);
 for(char &c:value)if(c>='A'&&c<='Z')c+=32;
 if(value=="*")return value;
 if(value.empty()||value.size()>31||value=="unknown"||value.front()=='-'||value.back()=='-')return "";
 bool hyphen=false;
 for(char c:value){if(c=='-'){if(hyphen)return "";hyphen=true;}else{if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')))return "";hyphen=false;}}
 return value;
}
}
