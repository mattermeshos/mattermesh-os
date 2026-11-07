#include <cassert>
#include <iostream>
#include "mesh/journal.h"
// A row cut short (full card, bus error) must not swallow the next one after a reboot (the stub FS cannot cut a write
// in half, so the in-session path of appendRow is the same code without this check).
static matter::Record message(uint32_t token){matter::Record r;r.token=token;r.chat=1;r.sender="Peer";r.body="m"+std::to_string(token);r.status="Received";r.scope="it";return r;}
int main(){
 fs::FS storage;matter::Journal journal;journal.begin(storage);assert(journal.append(message(1))&&journal.append(message(2)));
 auto &file=*storage.files["/mattermesh/messages.tsv"];file.resize(file.rfind('\n',file.size()-2)+12);// row 2 cut after 11 bytes, no new line
 matter::Journal reboot;reboot.begin(storage);assert(reboot.summary(1)->total==1);
 assert(reboot.append(message(3)));matter::Journal again;again.begin(storage);assert(again.summary(1)->total==2&&again.summary(1)->last.token==3);
 matter::Record r;assert(again.readRecord(3,r,false)&&r.body=="m3");
 std::cout<<"PASS: a row cut short is closed by the next append (after a reboot); later rows are read back\n";
}
