#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <string>
#include <vector>
namespace device {
// Link viewer (Plus): opens an http(s) link from a chat over Wi-Fi on a worker task (core 0).
// - JPEG (baseline) and PNG photos are decoded into a PSRAM picture of at most LinkImageMaxW x LinkImageMaxH
//   (downscaled on decode; RGB565 in panel byte order), kept until the next open or close.
// - HTML pages become plain text (title, readable text, up to LinkMaxLinks links, og:image / twitter:image).
// No scripts, no cookies, no forms. Downloads are capped (LinkMaxImageBytes / LinkMaxPageBytes); https uses the
// built-in root CA bundle (services/ca_bundle.h) with full certificate checks.
constexpr int LinkImageMaxW=640,LinkImageMaxH=480,LinkMaxLinks=40;
constexpr size_t LinkMaxImageBytes=3u*1024*1024,LinkMaxPageBytes=1024u*1024,LinkMaxText=24u*1024;// a longer page shows its first LinkMaxPageBytes
enum class LinkPhase {Idle,Loading,Image,Page,Error};
struct LinkTarget {std::string text,url;};
struct LinkState {
 LinkPhase phase=LinkPhase::Idle;
 std::string url,host,title,message,text,imageUrl;// text: page body, '\n' between blocks; imageUrl: og:image
 std::vector<LinkTarget> links;
 uint32_t received=0,total=0;// bytes of the download in progress (total 0 = unknown)
 int imageW=0,imageH=0,sourceW=0,sourceH=0;// decoded picture / original size
 bool page=false;std::string pictureError;// page text kept next to a picture (photo pages); why the og:image did not open
 uint32_t revision=0;
};
void linkBegin();
bool linkOpen(const std::string &url);// false: not an http(s) URL or the worker is unavailable
void linkClose();// cancels a download in progress and frees the picture / page
LinkState linkState();// full copy (page text and links included): for building the page rows once per revision
// What key / touch handling needs, without the page text and links.
struct LinkMeta {LinkPhase phase=LinkPhase::Idle;bool page=false;std::string url,host,title,message,imageUrl;int imageW=0,imageH=0,sourceW=0,sourceH=0,links=0;uint32_t received=0,total=0,revision=0;};
LinkMeta linkMeta();
uint32_t linkRevision();
// The decoded picture with its size, read in one locked step; null unless phase==Image. Valid until
// linkOpen/linkClose (both run on the UI thread, the only reader).
const uint16_t *linkPicture(int &w,int &h);
// First http(s) URL in a chat text ("www." gets https://), trailing punctuation dropped; "" when none.
std::string linkFind(const std::string &text);
// Memory report for the serial console: internal heap free/min, PSRAM free, picture/page bytes held.
std::string linkMemory();
}
