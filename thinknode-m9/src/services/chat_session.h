#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>
#include "scope_policy.h"
namespace device {
// RAM-only drafts. Opening/closing a conversation transfers ownership of its
// editor text; a sent/cleared draft can never be resurrected by a redraw.
template<class Text,size_t N> struct ChatDrafts {
 Text saved[N];
 bool valid(int chat)const{return chat>=0&&size_t(chat)<N;}
 void save(int chat,const Text &text){if(valid(chat))saved[chat]=text;}
 void leave(int chat,Text &text){save(chat,text);text=Text();}
 void clear(int chat){if(valid(chat))saved[chat]=Text();}
 void take(int chat,Text &text){text=valid(chat)?saved[chat]:Text();clear(chat);}
 void recoverIfEmpty(int chat,Text &text){if(valid(chat)&&!text.length()&&saved[chat].length())take(chat,text);}
};
struct ResendPayload {bool valid=false,scoped=false;std::string body,scope,error="Select your sent message";};
template<class Message,class Records>
ResendPayload resendPayload(const Message &selected,int selectedChat,int activeChat,const Records &records){
 ResendPayload out;if(!selected.mine||selectedChat<0||selectedChat!=activeChat)return out;
 out.body=selected.body.c_str();std::string scope=selected.scope.c_str();
 for(auto &r:records)if(selected.token&&r.token==selected.token&&r.mine&&r.chat==activeChat){out.body=r.body.c_str();scope=r.scope.c_str();break;}
 out.scope=normalizedScope(scope);out.scoped=!scope.empty();
 if(out.scoped&&out.scope.empty()){out.error="Original region scope invalid";return out;}
 out.valid=!out.body.empty();if(out.valid)out.error.clear();return out;
}
// Persist first. A failed SD write must not decrement the in-memory unread
// count or hide that RX from the lock-screen browser.
template<class Journal>
bool commitPaintedRead(Journal &journal,int chat,uint32_t painted,uint32_t latest,uint16_t paintedUnread,uint16_t &unread){
 if(!journal.markRead(chat,painted))return false;
 unread=latest==painted?0:(unread>=paintedUnread?unread-paintedUnread:0);return true;
}
}
