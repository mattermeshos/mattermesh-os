#pragma once
#include <Arduino.h>
#include "power_limits.h"
#include "discovery_limits.h"
#include "flood_policy.h"
#include "message_limits.h"
#include "rx_log.h"
#include <FS.h>
#include <ExtensionIOXL9555.hpp>
#include <vector>
namespace matter {
struct Config { float frequency=869.618f,bandwidth=62.5f;uint8_t sf=8,cr=8,pathBytes=OUTGOING_PATH_HASH_BYTES;int8_t power=MAX_TX_POWER;String name="MatterMesh"; };
struct Node {String key,name;uint8_t type=0;uint32_t seen=0,advertAt=0;int hops=-1;float rssi=0,snr=0;bool live=false;int32_t lat=0,lon=0;};
struct NodePin {double lat=0,lon=0;bool repeater=false,live=false;char name[16]={};};
struct Received {int channelId=-1;String key,channel,sender,body,scope,path;uint32_t timestamp=0;int hops=-1;bool direct=false;};
struct Delivery {bool txComplete=false,txSuccess=false;uint32_t token=0;String state,heard,firstRepeater;int repeaters=-1;};
struct Snapshot {bool ready=false,identity=false,internal=true,rx=false,tx=false,suspended=false;/* suspended: asleep for the companion link */int error=0,nodes=0,queued=0;uint32_t received=0,sent=0,errors=0;float rssi=0,snr=0;Config config;};
using Receiver=void(*)(const Received&);using Delivered=void(*)(const Delivery&);
bool begin(fs::FS &fs,ExtensionIOXL9555 &io,bool ioReady,Receiver receiver,Delivered delivered);
struct RadioDiag {float tcxo=0,rssi=0,snr=0;bool boosted=false;uint16_t deviceErrors=0;int noiseFloor=0;uint32_t received=0,recvErrors=0;};
RadioDiag radioDiag();bool radioClearErrors();
// Noise floor (MeshCore calibrates it every few seconds) and the last packet's signal, no SPI access.
struct RadioLevels {int noiseFloor=0;float rssi=0,snr=0;bool received=false;};RadioLevels radioLevels();
struct RadioCounters {bool ready=false;uint32_t received=0,sent=0;};// the main loop polls these every iteration
void loop();Snapshot snapshot();RadioCounters radioCounters();std::vector<Node> nodes(int filter,int offset=0,int limit=NODE_CAPACITY);int nodeCount(int filter=0);
// Contacts page: live nodes first (most recently heard), then saved ones by name; `query` filters names case-insensitively.
struct ContactPage {std::vector<Node> rows;int total=0,live=0;};ContactPage contacts(int filter,const std::string &query,int offset,int limit);int contactsCount(int filter,const std::string &query);
// Contacts menu. Add new / Replace oldest are the MeshCore app's manual add and overwrite settings. isProtected: contacts
// Delete oldest and Replace keep (a chat is open with them). contactsDeleteOldest / All return how many went.
bool contactsAddNew();bool contactsSetAddNew(bool on);bool contactsReplaceOldest();bool contactsSetReplaceOldest(bool on);
void contactsProtect(bool (*isProtected)(const char *key));int contactsDeletable();int contactsDeleteOldest(int count);int contactsDeleteAll();int liveCount(int filter,uint32_t withinMs);int nearestNodePins(double lat,double lon,NodePin *out,int capacity);/* positioned nodes, nearest first */int liveBestSnr(uint32_t withinMs,float &best);/* nodes heard within withinMs and their best SNR */bool latestLive(int filter,Node &out);// Home widgets: no sort, no allocation
int nodePins(double south,double west,double north,double east,NodePin *out,int capacity);
int repeaterPinsForPrefix(const String &prefix,NodePin *out,int capacity,int &matches);int nodePinsForName(const String &name,NodePin *out,int capacity,int &matches);
String nodeNameForPrefix(const String &prefix,int &matches,int type=0);
int antennaSelectLevel();bool antennaSavedInternal();bool antenna(bool internal,String &reason,bool confirmed=false);
bool configure(const Config &config,String &reason);
bool channel(int id,const String &name,const String &key,const String &scope,String &reason);
void clearChannels();
// Secret actually configured for a channel slot (also derived #hashtag / Public keys), 32 hex.
bool channelSecret(int id,String &hex);
// Saved route to a contact (MeshCore out_path, kept in contacts.tsv); false if the contact is unknown.
bool contactPath(const String &key,uint8_t &encoded,uint8_t *bytes,size_t capacity);
// Route of the direct messages to a contact. Auto: the path MeshCore learns from replies (flood until then; a learned
// path that gets no ACK is dropped and the message goes once more by flood). Flood: always flood, the learned path is
// not used. Manual: a path set by hand (hops of 1-3 byte repeater hashes), kept when replies would teach another.
enum class DmRouteMode : uint8_t {Auto=0,Flood=1,Manual=2};
DmRouteMode dmRouteMode(const String &key);
bool dmRouteReset(const String &key);// Auto again, the learned path forgotten
bool dmRouteFlood(const String &key,bool on);
bool dmRouteSet(const String &key,const uint8_t *hashes,int hops,int hashBytes);
// Known repeaters for a path: heard live first, then the most recently heard (at most limit).
std::vector<Node> recentRepeaters(int limit);
 bool registerScope(const String &scope);
bool send(int channelId,const String &peerKey,const String &scope,const String &text,uint32_t token,String &reason);
size_t textLimit(bool direct);
bool advertise(String &reason,bool position=false,double lat=0,double lon=0);bool busy();
// Changes of the node cache (adverts, names, positions): the Contacts page repaints when it moves.
uint32_t nodesRevision();

// Companion link: the SX1262 sleeps (no RX, no TX, the mesh loop idles) while another companion carries the traffic;
// false wakes it with the saved settings.
bool radioSuspend(bool off);
// The last advert sent and the repeaters whose copy of it came back (hashes: hex, comma separated).
struct AdvertHeard {bool sent=false;uint32_t at=0,revision=0;int count=0;String hashes;};AdvertHeard advertHeard();
// Companion link: a channel send that left from the companion's radio (token, the 16-byte channel key, the sent
// timestamp and text), and each packet the companion heard (its RX log): the repeats of the send report Heard xN.
void remoteEchoTrack(uint32_t token,const uint8_t secret[16],uint32_t timestamp,const String &text);
void remoteRaw(const uint8_t *raw,int len);

bool syncClock(uint32_t epoch,String &reason);
String ownKey();
// Every packet the radio decoded, newest first (CLI "log start").
const RxLog &rxLog();
// MeshCore companion app protocol over BLE, USB and Wi-Fi (src/mesh/companion.inc). One command frame in, replies
// through `write`; companionPump() streams a running contact listing one frame per call.
constexpr size_t COMPANION_FRAME_MAX=176;
using CompanionWriter=void(*)(void *ctx,const uint8_t *frame,size_t len);
struct CompanionSession {bool listing=false,active=false;int next=0;uint32_t since=0,mostRecent=0,pushSeq=0,logSeq=0;};// active: the app sent DEVICE_QUERY on this link
struct CompanionHooks {uint16_t (*batteryMv)()=nullptr;uint32_t (*storageUsedKb)()=nullptr,(*storageTotalKb)()=nullptr;void (*reboot)()=nullptr;
 uint32_t (*pin)()=nullptr;bool (*setPin)(uint32_t)=nullptr;void (*location)(int32_t &lat,int32_t &lon)=nullptr;
 bool (*setLocation)(int32_t lat,int32_t lon)=nullptr;// CMD_SET_ADVERT_LATLON (1e-6 degrees): the manual position
 // App channel edit (CMD_SET_CHANNEL): 0 = saved, otherwise a companion error code.
 uint8_t (*setChannel)(uint8_t idx,const char *name,const uint8_t secret[16])=nullptr;};
void companionHooks(const CompanionHooks &hooks);
void companionFrame(CompanionSession &session,const uint8_t *frame,size_t len,CompanionWriter write,void *ctx);
bool companionPump(CompanionSession &session,CompanionWriter write,void *ctx);
}
