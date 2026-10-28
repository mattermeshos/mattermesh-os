#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "services/hex_util.h"
#include "qr/qrcodegen.h"

// Channel sharing QR in the MeshCore app format (meshcore-base/docs/qr_codes.md):
// meshcore://channel/add?name=<url>&secret=<32 hex>[&region_scope=<url>]
namespace mm {
inline std::string urlEncode(const std::string &s){std::string out;for(unsigned char c:s){if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=char(c);else{out+='%';out+=hexDigit(c>>4,true);out+=hexDigit(c,true);}}return out;}
inline std::string channelShareUrl(const std::string &name,const std::string &secretHex,const std::string &scope){std::string url="meshcore://channel/add?name="+urlEncode(name)+"&secret="+secretHex;if(!scope.empty()&&scope!="*")url+="&region_scope="+urlEncode(scope);return url;}
constexpr int QrMaxVersion=10;
// Row-major modules (1 = dark); returns the side length, 0 if the text does not fit.
inline int encodeQr(const std::string &text,std::vector<uint8_t> &modules){
 uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(QrMaxVersion)],temp[qrcodegen_BUFFER_LEN_FOR_VERSION(QrMaxVersion)];modules.clear();
 if(!qrcodegen_encodeText(text.c_str(),temp,qr,qrcodegen_Ecc_MEDIUM,qrcodegen_VERSION_MIN,QrMaxVersion,qrcodegen_Mask_AUTO,true))return 0;
 int n=qrcodegen_getSize(qr);modules.resize(n*n);for(int y=0;y<n;y++)for(int x=0;x<n;x++)modules[y*n+x]=qrcodegen_getModule(qr,x,y);return n;
}
}
