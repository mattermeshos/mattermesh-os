#include <cassert>
#include <cstring>
#include <iostream>
#include "ui/renderer.h"
#include "ui/fast_canvas.h"
#include "services/unlock_taps.h"
#include "services/scope_policy.h"
#include "services/lock_policy.h"
int main(){
 using device::normalizedScope;
 for(auto name:{"it","it-vda","eu","us-ca","*","region123"})assert(normalizedScope(name)==name);
 assert(normalizedScope(" #IT-VDA ")=="it-vda");
 for(auto name:{"","--","it--lig","-it","it-","it lig","unknown","it/lig","🇮🇹"})assert(normalizedScope(name).empty());
 assert(normalizedScope(std::string(32,'a')).empty());
 device::UnlockTaps taps;assert(taps.press(100));assert(!taps.press(200));assert(!taps.expire(10000));assert(!taps.release(10000));assert(taps.taps==1);assert(!taps.press(10010));assert(taps.press(10100));assert(taps.taps==2&&taps.release(10200));
 taps.reset();assert(taps.press(11000));assert(!taps.release(11100));assert(taps.expire(11100+device::UNLOCK_TAP_WINDOW_MS+1));assert(taps.taps==0);
 taps.reset();assert(taps.press(15000));assert(!taps.release(15010)&&taps.taps==0);assert(taps.press(15050));assert(!taps.release(15100));assert(taps.press(18000)&&taps.taps==1);
 device::UnlockTaps wrap;assert(wrap.press(0xfffffff0));assert(!wrap.release(50));assert(wrap.press(100));assert(wrap.release(180));
 // Plus: 80 MHz floor (the octal PSRAM loses timing on the 40 MHz crystal and resets), so the locked clock is 80 MHz idle or busy, never the 240 MHz normal one.
 static_assert(device::LOCKED_CPU_FREQUENCY==80&&device::LOCKED_CPU_FALLBACK>=device::LOCKED_CPU_FREQUENCY&&device::LOCKED_CPU_FALLBACK<device::NORMAL_CPU_FREQUENCY,"Plus locked CPU floor");
 mm::FastCanvas16 canvas(mm::W,mm::H);constexpr size_t Px=size_t(mm::W)*mm::H,Row=mm::W;auto frame=[&]{return std::vector<uint16_t>(canvas.getBuffer(),canvas.getBuffer()+Px);};mm::Renderer r(canvas);mm::State s;s.page=mm::Page::Chat;s.chatName="Italia";s.messageMenu=true;r.draw(s);
 for(int i=0;i<3;i++){auto rect=mm::chatOptionRect(i);assert(rect.x>=mm::Margin&&rect.x+rect.w<=mm::W-mm::Margin&&rect.y>=mm::Top&&rect.y+rect.h<mm::Bottom);}
 s.messageMenu=false;s.scopePicker=true;s.regionScopes={"*","it","it-vda","it-pie","it-tos","eu"};s.scopeFocus=2;r.draw(s);s.scopeEditing=true;s.scopeInput="it-vda";r.draw(s);s.scopePicker=s.scopeEditing=false;s.clearChatConfirm=true;r.draw(s);
 s=mm::State{};r.lock(s);auto idle=frame();s.unlockTapCount=1;r.lock(s);auto one=frame();s.unlockTapCount=2;r.lock(s);assert(memcmp(one.data(),canvas.getBuffer(),Px*2));
 for(int y=0;y<mm::H;y++)if(y<mm::HoldBannerY||y>=mm::HoldBannerY+mm::HoldBannerH)assert(!memcmp(idle.data()+y*Row,canvas.getBuffer()+y*Row,Row*2));
 std::cout<<"PASS: scope normalization/invalid names, two distinct debounced taps/held-key/bounce/timeout/wrap, read-message Home route, 80 MHz locked CPU floor, menu/scope/clear sheets, 1/2 and 2/2 top-only banners\n";
}
