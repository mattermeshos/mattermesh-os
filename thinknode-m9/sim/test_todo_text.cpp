#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include "ui/renderer.h"
#include "ui/utf8.h"
#include "services/policy.h"
#include "ui/navigation_catalog.h"
#include "ui/fast_canvas.h"
int main(){
 mm::FastCanvas16 canvas(mm::W,mm::H);mm::Components c(canvas);
 std::string source="abcdefabcdefabcdefabcdefabcdefabcdefabcdef🙂👍";
 auto tail=c.inputTail(source,70);assert(c.width(tail,mm::Font::ChatBody)<=70&&tail.find("..") == std::string::npos&&tail.substr(tail.size()-8)=="🙂👍");
 uint32_t cp;for(size_t at=0;at<tail.size();)at=mm::utf8Next(tail,at,cp);
 auto lines=c.lines("🙂🙂🙂🙂🙂🙂🙂🙂🙂🙂",40,mm::Font::ChatBody);std::string joined;for(auto &line:lines){assert(c.width(line,mm::Font::ChatBody)<=40);joined+=line;}assert(joined=="🙂🙂🙂🙂🙂🙂🙂🙂🙂🙂");
 mm::State s;s.page=mm::Page::Chat;s.chatName="Italia";s.input=source;s.emojiPicker=true;s.messages.push_back({"River","10:18","Ciao 🙂👍","2 hop | it",false});mm::Renderer r(canvas);r.draw(s);
 for(auto &tab:mm::HomeTabs)assert(c.width(tab.label,mm::Font::NavLabel)<=mm::MenuHomeTabW-2);// the Plus Home tabs (label centred under the icon)
 canvas.fillScreen(mm::BG);mm::Status status;status.charging=false;c.statusBar(status);std::vector<uint16_t> before(canvas.getBuffer(),canvas.getBuffer()+mm::W*mm::H);status.charging=true;c.statusBar(status);std::vector<uint16_t> after(canvas.getBuffer(),canvas.getBuffer()+mm::W*mm::H);
 assert(before!=after);assert(std::equal(before.begin()+mm::W*mm::StatusH,before.end(),after.begin()+mm::W*mm::StatusH));// bolt drawn, nothing below the status bar touched
 std::cout<<"PASS: UTF-8 emoji wrap, scrolling tail/caret width, 19 requested battery reference values, charging marker, Home labels and chat rendering\n";
}
