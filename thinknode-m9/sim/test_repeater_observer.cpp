#include <cassert>
#include <iostream>
#include "mesh/repeater_observer.h"
int main(){matter::RepeaterObserver r;uint8_t a[]={0xab,0xcd,0xab};assert(r.observe(a,3,1)&&r.count==2);assert(!r.observe(a,3,1)&&r.count==2);uint8_t b[]={0xcd,0xef};assert(r.observe(b,2,1)&&r.count==3);uint8_t c[]={0xab,0xcd};assert(r.observe(c,1,2)&&r.count==4);assert(!r.observe(c,1,0));assert(!r.observe(c,1,5));assert(!r.observe(c,64,2));assert(!r.observe(c,0,1));std::cout<<"PASS: distinct path hashes, duplicate echoes, variable hash widths, empty and invalid paths\n";}
