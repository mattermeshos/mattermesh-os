#include "sd_transfer.h"
#include "sd_replace.h"
#include "psram.h"
#include <SD.h>
#include <esp_crc.h>
#include <esp_heap_caps.h>
namespace device {
namespace {
constexpr size_t CHUNK=SD_TRANSFER_CHUNK;
// Every protocol line is flushed: the Arduino HWCDC can leave the last line in its TX ring until the next write (seen
// as an ACK arriving 10 s late, with the SD_PUT_TIMEOUT that followed it).
void put(const String &path,uint32_t bytes){
 if(!path.startsWith("/")||!bytes){Serial.println("SD_PUT_ERROR args");Serial.flush();return;}
 String part=path+".part";SD.remove(part);
 for(int slash=path.indexOf('/',1);slash>0;slash=path.indexOf('/',slash+1)){String dir=path.substring(0,slash);if(!SD.exists(dir))SD.mkdir(dir);}// FAT mkdir is one level
 File f=SD.open(part,FILE_WRITE);if(!f){Serial.println("SD_PUT_ERROR open");Serial.flush();return;}
 uint8_t *buf=static_cast<uint8_t*>(psramMalloc(CHUNK));
 if(!buf){f.close();SD.remove(part);Serial.println("SD_PUT_ERROR memory");Serial.flush();return;}
 // The RX queue is sized once at boot (main.cpp); resizing it live races with the USB ISR.
 Serial.printf("SD_PUT_READY %lu %u\n",(unsigned long)bytes,unsigned(CHUNK));Serial.flush();
 uint32_t done=0,crc=0;bool ok=true;int resends=0;
 while(done<bytes){
  // Block reads (HWCDC has no RX flow control: the queue must be drained promptly, not byte by byte).
  // A chunk that starts but stops short for 3 s lost bytes (the USB ISR drops what it cannot queue); one that never
  // starts may just be late: 10 s before giving up.
  size_t want=std::min<size_t>(CHUNK,bytes-done),got=0;uint32_t start=millis(),last=start;
  while(got<want&&int32_t(millis()-(got?last+3000:start+10000))<0){size_t n=Serial.read(buf+got,want-got);if(n){got+=n;last=millis();}else vTaskDelay(1);}
  if(got!=want){
   // Discard the partial chunk once the line is quiet and ask for it again.
   if(!got||++resends>SD_TRANSFER_RESENDS){ok=false;Serial.printf("SD_PUT_TIMEOUT %lu\n",(unsigned long)done);Serial.flush();break;}
   for(uint32_t quiet=millis();int32_t(millis()-quiet)<300;)if(Serial.read(buf,CHUNK))quiet=millis();else vTaskDelay(1);
   Serial.printf("SD_PUT_RESEND %lu\n",(unsigned long)done);Serial.flush();continue;}
  if(f.write(buf,got)!=got){ok=false;Serial.printf("SD_PUT_WRITE_ERROR %lu\n",(unsigned long)done);Serial.flush();break;}
  crc=esp_crc32_le(crc,buf,got);done+=got;
  Serial.printf("SD_PUT_ACK %lu %08lx\n",(unsigned long)done,(unsigned long)crc);Serial.flush();
 }
 f.flush();f.close();free(buf);
 // Replace without keeping a .bak (uploads can be large); the old file comes back when the rename fails.
 if(ok){String bak=path+".bak";ok=device::sdReplace(SD,part.c_str(),path.c_str(),bak.c_str(),false);if(!ok)Serial.println("SD_PUT_ERROR rename");Serial.flush();}
 if(!ok)SD.remove(part);
 Serial.printf("%s %lu %08lx\n",ok?"SD_PUT_DONE":"SD_PUT_FAIL",(unsigned long)done,(unsigned long)crc);Serial.flush();
}
void crc(const String &path){
 File f=SD.open(path,FILE_READ);if(!f||f.isDirectory()){Serial.printf("SD_CRC missing %s\n",path.c_str());return;}
 uint8_t *buf=static_cast<uint8_t*>(psramMalloc(CHUNK));
 if(!buf){f.close();Serial.println("SD_CRC error memory");return;}
 uint32_t c=0,total=0;uint32_t t=millis();
 while(f.available()){int n=f.read(buf,CHUNK);if(n<=0)break;c=esp_crc32_le(c,buf,n);total+=n;if((total&0xfffff)==0)yield();}
 f.close();free(buf);
 Serial.printf("SD_CRC ok %s bytes=%lu crc=%08lx ms=%lu\n",path.c_str(),(unsigned long)total,(unsigned long)c,(unsigned long)(millis()-t));
}
void list(const String &dir){
 File d=SD.open(dir.length()?dir:"/");if(!d||!d.isDirectory()){Serial.printf("SD_LS missing %s\n",dir.c_str());return;}
 Serial.printf("SD_LS_BEGIN %s\n",dir.c_str());
 for(File e=d.openNextFile();e;e=d.openNextFile()){Serial.printf("SD_LS %s %s %lu\n",e.isDirectory()?"dir":"file",e.name(),(unsigned long)e.size());e.close();}
 d.close();Serial.println("SD_LS_END");
}
}
bool sdTransferCommand(const String &s){
 if(!s.startsWith("sd "))return false;
 if(s.startsWith("sd put ")){int sp=s.indexOf(' ',7);if(sp<0){Serial.println("SD_PUT_ERROR args");return true;}put(s.substring(7,sp),strtoul(s.c_str()+sp+1,nullptr,10));return true;}
 if(s.startsWith("sd crc ")){crc(s.substring(7));return true;}
 if(s.startsWith("sd ls")){String d=s.substring(5);d.trim();list(d);return true;}
 // sd mv <from> <to> (files or folders, no spaces in paths); sd rmdir <empty folder>
 if(s.startsWith("sd mv ")){int sp=s.indexOf(' ',6);if(sp<0){Serial.println("SD_MV_ERROR args");return true;}String from=s.substring(6,sp),to=s.substring(sp+1);Serial.printf("SD_MV %s %s %d\n",from.c_str(),to.c_str(),from.startsWith("/")&&to.startsWith("/")&&SD.rename(from,to));return true;}
 if(s.startsWith("sd rmdir ")){String p=s.substring(9);Serial.printf("SD_RMDIR %s %d\n",p.c_str(),p.startsWith("/")&&SD.rmdir(p));return true;}
 if(s.startsWith("sd rm ")){String p=s.substring(6);Serial.printf("SD_RM %s %d\n",p.c_str(),p.startsWith("/")&&SD.remove(p));return true;}
 Serial.println("SD_UNKNOWN");return true;
}
}
