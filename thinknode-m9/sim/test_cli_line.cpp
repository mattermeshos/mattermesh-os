#include <cassert>
#include <iostream>
#include "services/cli_line.h"
using namespace device;
int main(){
 CliLine line;assert(!line.erase());line.insert("ver");line.left();line.left();line.insert("X");assert(line.text=="vXer"&&line.cursor==2);line.erase();assert(line.text=="ver"&&line.cursor==1);
 line.set("a\xc3\xa8z");line.left();line.left();assert(line.cursor==1);line.right();assert(line.cursor==3);line.erase();assert(line.text=="az"&&line.cursor==1);
 line.set(std::string(CLI_INPUT_MAX,'x'));assert(!line.insert("y"));
 CliHistory h;CliLine l;assert(!h.older(l));h.add("ver");h.add("clock");h.add("clock");assert(h.size()==2);l.set("dra");assert(h.older(l)&&l.text=="clock"&&h.older(l)&&l.text=="ver"&&!h.older(l));assert(h.newer(l)&&l.text=="clock"&&h.newer(l)&&l.text=="dra"&&!h.newer(l));
 for(int i=0;i<40;i++)h.add("c"+std::to_string(i));assert(h.size()==CLI_HISTORY_MAX);
 std::vector<std::string> words={"get radio","get name","get tx","help","ver"},m;CliLine c;c.set("he");assert(cliComplete(c,words,m)&&c.text=="help "&&m.empty());
 c.set("get ");assert(!cliComplete(c,words,m)&&m.size()==3);c.set("g");assert(cliComplete(c,words,m)&&c.text=="get "&&m.size()==3);c.set("get r");assert(cliComplete(c,words,m)&&c.text=="get radio ");
 c.set("zz");assert(!cliComplete(c,words,m)&&m.empty());
 auto w=cliWords("  set  radio 869.6,62.5 ");assert(w.size()==3&&w[0]=="set"&&w[2]=="869.6,62.5");
 std::cout<<"PASS: CLI cursor editing (UTF-8), length cap, history browse/draft/cap, completion prefix/candidates, word split\n";}
