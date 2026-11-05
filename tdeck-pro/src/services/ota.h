#pragma once
#include <Arduino.h>
namespace device {
// Over-the-air updates from the MatterMesh OTA server (tools/ota_server on the NAS).
// A release is accepted only for this board, newer than MM_VERSION_CODE, and signed with the
// ed25519 key of tools/ota_publish.py (Full tree, tdeck-max); the image is written to the idle OTA slot and booted
// once in "pending verify": a crash before otaLoop() marks it healthy rolls back to the old one.
enum class OtaPhase:uint8_t {Idle,Checking,UpToDate,Available,Downloading,Verifying,Restarting,Error};
struct OtaState {OtaPhase phase=OtaPhase::Idle;String message,version,notes,server;int32_t code=0;uint32_t size=0,done=0,checkedAt=0,revision=0;bool rolledBack=false,installed=false;int32_t refused=0;};// refused: build no longer offered (two rollbacks)
constexpr uint32_t OTA_FIRST_CHECK_MS=20000UL,OTA_CHECK_EVERY_MS=6UL*3600UL*1000UL,OTA_HEALTHY_AFTER_MS=30000UL;
const char *otaBoard();
String otaUnlockCode();
bool otaAppsUnlocked();// Full image with a valid license for this T-Deck; false on the Free image// XXXX-XXXX: identifies this T-Deck in a Ko-fi order (apps unlock)
void otaBegin();
// uiReady: the UI painted at least once. Marks the running image healthy, reports to the server and schedules checks.
void otaLoop(bool wifiConnected,bool uiReady);
bool otaCheck();// false: not started (a check or an install is running, no Wi-Fi, no server or no worker task)
bool otaInstall();
OtaState otaState();
uint32_t otaRevision();
// Space-separated base URLs tried in order (LAN first, then an external one).
// Addresses are never printed or drawn (the device must not reveal where it updates from): only their count.
int otaServerCount();void otaSetServers(const String &servers);
}
