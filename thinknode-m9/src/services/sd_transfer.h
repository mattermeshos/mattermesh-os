#pragma once
#include <Arduino.h>
// USB->SD upload chunk; the Serial RX queue holds two of them (sized at boot).
constexpr unsigned SD_TRANSFER_CHUNK=4096;
constexpr int SD_TRANSFER_RESENDS=8;// chunks asked again (SD_PUT_RESEND) per upload before SD_PUT_TIMEOUT
namespace device {
// USB maintenance commands for files on the SD card (used by the host tools):
//   sd put <path> <bytes>   raw upload in acknowledged 4 KiB chunks, missing folders created, safe replace at the end
//   sd crc <path>           CRC-32 and size of a file
//   sd ls <dir>             directory listing
//   sd rm <path>            delete a file
//   sd mv <from> <to>       rename / move a file or folder (no spaces in paths)
//   sd rmdir <dir>          delete an empty folder
// Returns true when the line was an SD command (handled, success or failure reported on USB).
bool sdTransferCommand(const String &line);
}
