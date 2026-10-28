#pragma once
#include <stddef.h>
#include <string>
namespace matter {
// BaseChatMesh MAX_TEXT_LEN, including "sender: " for channel messages.
// service.cpp asserts this against the MeshCore version actually compiled.
constexpr size_t MESSAGE_MAX_TEXT_BYTES=160;
constexpr size_t MESSAGE_SENDER_SEPARATOR_BYTES=2;
constexpr size_t messageBodyLimit(bool direct,size_t senderNameBytes){
 return direct?MESSAGE_MAX_TEXT_BYTES:
  senderNameBytes>=MESSAGE_MAX_TEXT_BYTES-MESSAGE_SENDER_SEPARATOR_BYTES?0:
  MESSAGE_MAX_TEXT_BYTES-senderNameBytes-MESSAGE_SENDER_SEPARATOR_BYTES;
}
inline std::string messageLimitError(size_t limit){return "Message full / "+std::to_string(limit)+"-byte limit";}
}
