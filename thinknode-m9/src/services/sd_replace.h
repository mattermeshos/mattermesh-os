#pragma once
// The one way every save replaces an SD file with its fully written temporary copy.
// The old file becomes the backup first and is put back when the final rename fails; readers open sdSavedPath(), so a
// power cut between the two renames (file missing, backup present) still finds the last good copy.
namespace device {
template<class FS> bool sdReplace(FS &fs,const char *tmp,const char *path,const char *bak,bool keepBackup=true){
 bool had=fs.exists(path);
 // Without the file an orphan backup is the only copy left: it is replaced only once the new file is in place.
 if(had){fs.remove(bak);if(!fs.rename(path,bak)){fs.remove(tmp);return false;}}
 if(!fs.rename(tmp,path)){if(had)fs.rename(bak,path);fs.remove(tmp);return false;}
 if(!keepBackup)fs.remove(bak);
 return true;
}
template<class FS> const char *sdSavedPath(FS &fs,const char *path,const char *bak){return fs.exists(path)?path:bak;}
}
