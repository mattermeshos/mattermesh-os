#include <cassert>
#include <iostream>
#include <set>
#include <random>
#include "services/channel_form.h"
int main(){
 const uint8_t bytes[16]={0x5a,0x1e,0x7c,0x90,0xb3,0xd2,0x4f,0x68,0xa0,0xc1,0xe9,0xb2,0x7d,0x4f,0x3a,0x85};
 auto hex=mm::channelSecretHex(bytes);assert(hex=="5a1e7c90b3d24f68a0c1e9b27d4f3a85"&&mm::validSecret(hex)&&!mm::zeroSecret(hex));
 assert(mm::channelSecretGroups(hex,0,16)=="5a1e 7c90 b3d2 4f68"&&mm::channelSecretGroups(hex,16,16)=="a0c1 e9b2 7d4f 3a85"&&mm::channelSecretGroups("abc",16,16).empty());
 std::string key;for(char c:std::string("5A1E 7C90-B3D24F68xyzA0C1E9B27D4F3A85"))mm::acceptSecretChar(key,c);assert(key==hex);assert(!mm::acceptSecretChar(key,'a'));
 assert(!mm::validSecret("5a1e")&&!mm::validSecret(hex+"0")&&!mm::validSecret("5A1E7C90B3D24F68A0C1E9B27D4F3A85")&&mm::zeroSecret(std::string(32,'0')));
 std::string name;for(char c:std::string("Breuil Centro_#1"))mm::acceptHashtagChar(name,c);assert(name=="breuil-centro1"&&mm::validHashtag(name));
 assert(!mm::validHashtag("")&&!mm::validHashtag("-x")&&!mm::validHashtag("x-")&&!mm::validHashtag("Upper")&&!mm::validHashtag(std::string(31,'a'))&&mm::validHashtag(std::string(30,'a')));
 std::string full(30,'a');assert(!mm::acceptHashtagChar(full,'b'));
 assert(mm::cleanChannelName("  #trailteam \t")=="#trailteam"&&mm::cleanChannelName(std::string(40,'x')).size()==31);
 std::mt19937 rng(7);std::set<std::string> seen;for(int i=0;i<1000;i++){uint8_t b[16];for(auto &v:b)v=rng();auto h=mm::channelSecretHex(b);assert(mm::validSecret(h));seen.insert(h);}assert(seen.size()==1000);
 std::cout<<"PASS: channel secret hex/grouping, typed key filter, hashtag rules, name cleaning\n";}
