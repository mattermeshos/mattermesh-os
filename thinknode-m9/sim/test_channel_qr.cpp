#include <cassert>
#include <iostream>
#include "services/channel_qr.h"
int main(){
 assert(mm::channelShareUrl("Public","8b3387e9c5cdea6ac9e5edbaa115cd72","*")=="meshcore://channel/add?name=Public&secret=8b3387e9c5cdea6ac9e5edbaa115cd72");
 auto url=mm::channelShareUrl("#trailteam","5a1e7c90b3d24f68a0c1e9b27d4f3a85","it-vda");
 assert(url=="meshcore://channel/add?name=%23trailteam&secret=5a1e7c90b3d24f68a0c1e9b27d4f3a85&region_scope=it-vda");
 assert(mm::urlEncode("Prova Aria/è")=="Prova%20Aria%2F%C3%A8");
 std::vector<uint8_t> m;int n=mm::encodeQr(url,m);assert(n>=21&&n<=57&&int(m.size())==n*n&&(n-17)%4==0);
 for(int y=0;y<7;y++)for(int x=0;x<7;x++){bool ring=x==0||y==0||x==6||y==6,core=x>=2&&x<=4&&y>=2&&y<=4;assert(m[y*n+x]==(ring||core));}// finder pattern
 auto big=mm::channelShareUrl(std::string(31,'x'),std::string(32,'a'),std::string(31,'y'));int worst=mm::encodeQr(big,m);assert(worst>0&&170/(worst+4)>=3);std::cout<<"trailteam QR "<<n<<" modules, worst case "<<worst<<"\n";
 assert(mm::encodeQr(std::string(400,'z'),m)==0&&m.empty());
 std::cout<<"PASS: MeshCore channel QR URL (encoding, optional scope), QR size/finder, worst-case fits, overflow rejected\n";}
