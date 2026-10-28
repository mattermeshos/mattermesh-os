#pragma once
// Conversations (channels and direct chats) share the MeshCore channel slots one to one. 32 at most: the per-chat
// flags (muted, read, cleared) are 32-bit masks. platformio.ini sets MAX_GROUP_CHANNELS.
#ifndef MAX_GROUP_CHANNELS
#define MAX_GROUP_CHANNELS 32
#endif
namespace matter {constexpr int CHAT_SLOTS=MAX_GROUP_CHANNELS;static_assert(CHAT_SLOTS<=32,"chat flags are 32-bit masks");}
