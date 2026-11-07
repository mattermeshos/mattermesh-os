#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include "services/sd_replace.h"
// Files as name -> content; renames listed in failRename fail once, as a full or worn SD card does.
struct MemFS {
 std::map<std::string,std::string> files;std::set<std::pair<std::string,std::string>> failRename;
 bool exists(const char *p){return files.count(p)>0;}
 bool remove(const char *p){return files.erase(p)>0;}
 bool rename(const char *a,const char *b){if(failRename.erase({a,b})||!files.count(a)||files.count(b))return false;files[b]=files[a];files.erase(a);return true;}
 std::string read(const char *path,const char *bak){auto p=device::sdSavedPath(*this,path,bak);return files.count(p)?files[p]:"";}
};
int main(){
 const char *tmp="/f.tmp",*path="/f.tsv",*bak="/f.bak";
 MemFS fs;fs.files[tmp]="v1";assert(device::sdReplace(fs,tmp,path,bak)&&fs.files[path]=="v1"&&!fs.exists(tmp)&&!fs.exists(bak));
 fs.files[tmp]="v2";assert(device::sdReplace(fs,tmp,path,bak)&&fs.files[path]=="v2"&&fs.files[bak]=="v1");
 fs.files[tmp]="v3";assert(device::sdReplace(fs,tmp,path,bak,false)&&fs.files[path]=="v3"&&!fs.exists(bak));
 // Rotating the old file fails: it stays untouched, the temporary copy is dropped.
 fs.files[tmp]="v4";fs.failRename.insert({path,bak});assert(!device::sdReplace(fs,tmp,path,bak)&&fs.files[path]=="v3"&&!fs.exists(tmp));
 // The new file cannot take its place: the old one comes back from the backup.
 fs.files[tmp]="v4";fs.failRename.insert({tmp,path});assert(!device::sdReplace(fs,tmp,path,bak)&&fs.files[path]=="v3"&&!fs.exists(tmp)&&!fs.exists(bak));
 // Power cut between the renames: only the backup is left and readers use it at boot.
 fs.files.erase(path);fs.files[bak]="v3";assert(fs.read(path,bak)=="v3");
 // A failing save then must not lose that orphan backup...
 fs.files[tmp]="v5";fs.failRename.insert({tmp,path});assert(!device::sdReplace(fs,tmp,path,bak)&&fs.read(path,bak)=="v3");
 // ...and a good one puts the new file first.
 fs.files[tmp]="v5";assert(device::sdReplace(fs,tmp,path,bak)&&fs.read(path,bak)=="v5");
 fs.files[tmp]="v6";assert(device::sdReplace(fs,tmp,path,bak)&&fs.files[path]=="v6"&&fs.files[bak]=="v5");
 std::cout<<"PASS: SD replace keeps/drops backup, failed rotate or rename restores the old file, orphan .bak read at boot and kept until a save succeeds\n";
}
