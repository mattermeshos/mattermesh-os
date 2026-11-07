#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include "services/connectivity.h"
static size_t allocations=0;
void* operator new(size_t n){allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}
int main(){
 device::ConnectivityState state;state.wifi=state.connected=true;state.ble=false;state.rssi=-65;state.ssid=std::string(32,'s');state.address="192.168.100.100";state.message="Connected to remembered network";state.revision=42;
 for(int i=0;i<24;i++){device::Network row;row.ssid=std::string(32,'a'+i);row.rssi=-40-i;state.networks.push_back(row);}
 allocations=0;auto full=state;size_t fullAllocations=allocations;
 allocations=0;auto lean=state.status();size_t leanAllocations=allocations;
 assert(lean.connected&&lean.wifi&&!lean.ble&&lean.rssi==-65&&lean.revision==42&&lean.ssid==state.ssid&&lean.message==state.message&&lean.networkCount==24);
 assert(full.networks.size()==24&&state.networks.size()==24);
 assert(fullAllocations>=leanAllocations+25);
 lean.ssid="changed";assert(state.ssid==std::string(32,'s'));
 std::cout<<"PASS: connectivity status retains scalar/string state without copying24 scan rows; allocations full="<<fullAllocations<<" status="<<leanAllocations<<"\n";
}
