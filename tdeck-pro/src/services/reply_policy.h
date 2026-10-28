#pragma once
#include "scope_policy.h"
namespace device {
enum class ReplyKind {Unavailable,Scoped,Reminder};
struct ReplyPlan {ReplyKind kind=ReplyKind::Scoped;std::string scope;int hops=-1;};
// Reply is always free text. Unknown scope inherits the conversation at send;
// it never turns a mention into an automatic service message.
inline ReplyPlan replyPlan(const std::string &scope,int hops,bool direct=false){ReplyPlan p;p.hops=hops>=0?hops:-1;p.scope=normalizedScope(scope);return p;}
inline std::string replyPrefix(const std::string &sender){return "@["+sender+"] ";}
}
