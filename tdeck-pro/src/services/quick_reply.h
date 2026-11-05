#pragma once
#include "../ui/utf8.h"
#include <stdint.h>
#include <string>
#include "reply_policy.h"
#include "mesh/message_limits.h"
namespace device {
constexpr uint32_t QUICK_REPLY_HOLD_MS=2000,QUICK_REPLY_DEBOUNCE_MS=30,QUICK_REPLY_LOST_RELEASE_MS=10000;
enum class EnterAction {None,Short,Hold};
struct LockEnter {
 bool down=false,consumed=false;uint32_t started=0,lastRelease=0;
 bool press(uint32_t now,bool hardware=false){if(down||(!hardware&&uint32_t(now-lastRelease)<QUICK_REPLY_DEBOUNCE_MS))return false;down=true;consumed=false;started=now;return true;}
 EnterAction tick(uint32_t now){if(!down||consumed||uint32_t(now-started)<QUICK_REPLY_HOLD_MS)return EnterAction::None;consumed=true;return EnterAction::Hold;}
 EnterAction release(uint32_t now,bool hardware=false){if(!down)return EnterAction::None;down=false;lastRelease=now;if(consumed)return EnterAction::None;consumed=true;auto elapsed=uint32_t(now-started);if(elapsed>=QUICK_REPLY_HOLD_MS)return EnterAction::Hold;return hardware||elapsed>=QUICK_REPLY_DEBOUNCE_MS?EnterAction::Short:EnterAction::None;}
 void reset(){down=consumed=false;started=lastRelease=0;}
 bool recover(uint32_t now){if(!down||uint32_t(now-started)<QUICK_REPLY_LOST_RELEASE_MS)return false;reset();return true;}
};
constexpr uint32_t QUICK_REPLY_CONFIRMATION_MS=1500;
struct ReplyOriginal {int chat=-1,hops=-1;uint32_t token=0;bool direct=false;std::string sender,body,scope,path,channel;};
struct QuickReply {
 size_t byteLimit=matter::MESSAGE_MAX_TEXT_BYTES;
 bool active=false,pending=false,sent=false,confirmationPainted=false;uint32_t txToken=0,confirmationSince=0;ReplyOriginal original;ReplyPlan plan;std::string text,error;
 bool open(const ReplyOriginal &selected,size_t limit=matter::MESSAGE_MAX_TEXT_BYTES){if(selected.chat<0||!selected.token)return false;byteLimit=limit;original=selected;plan=replyPlan(selected.scope,selected.hops,selected.direct);active=true;pending=sent=confirmationPainted=false;txToken=confirmationSince=0;
  text=replyPrefix(selected.sender);error="";return true;}
 bool editable()const{return active&&!pending&&!sent;}
 void append(char ch){if(!editable())return;if(text.size()<byteLimit){text+=ch;error="";}else error=matter::messageLimitError(byteLimit);}
 void appendText(const std::string &s){if(!editable())return;if(text.size()+s.size()<=byteLimit){text+=s;error="";}else error=matter::messageLimitError(byteLimit);}
 void erase(){if(!editable()||text.empty())return;text.erase(mm::utf8PrevStart(text,text.size()));error="";}
 void close(){active=pending=sent=confirmationPainted=false;txToken=confirmationSince=0;text.clear();error.clear();original=ReplyOriginal{};}
 void didPaint(uint32_t now){if(active&&sent&&!confirmationPainted){confirmationPainted=true;confirmationSince=now;}}
 bool confirmationExpired(uint32_t now)const{return active&&sent&&confirmationPainted&&uint32_t(now-confirmationSince)>=QUICK_REPLY_CONFIRMATION_MS;}
 bool complete(uint32_t token,bool success,const std::string &reason="TX failed / try again"){
  if(!active||!pending||!token||token!=txToken)return false;
  pending=false;txToken=0;
  if(success){sent=true;confirmationPainted=false;error.clear();}else error=reason;return true;
 }
 template<class Sender>bool send(Sender sender){
  if(!editable())return false;
  auto body=text;
  auto prefix=replyPrefix(original.sender);auto content=body.compare(0,prefix.size(),prefix)==0?body.substr(prefix.size()):body;
  if(content.find_first_not_of(" \r\n\t")==std::string::npos){error="Write a reply first";return false;}
  if(body.size()>byteLimit){error=matter::messageLimitError(byteLimit);return false;}
  std::string reason;if(!sender(original.chat,plan.scope,body,reason)){error=reason.empty()?"Not sent / try again":reason;return false;}pending=true;error="Sending...";return true;
 }
};
}
