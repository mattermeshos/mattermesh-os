#pragma once
#include <Arduino.h>
#include <map>
#include <memory>
class File {
 std::shared_ptr<std::string> bytes;size_t cursor=0;bool *writeFailure=nullptr;
public:
 File()=default;File(std::shared_ptr<std::string> value,bool *failure=nullptr):bytes(value),writeFailure(failure){}
 explicit operator bool()const{return bool(bytes);}bool available(){return bytes&&cursor<bytes->size();}
 size_t position()const{return cursor;}size_t size()const{return bytes?bytes->size():0;}
 bool seek(size_t value){if(!bytes||value>bytes->size())return false;cursor=value;return true;}
 String readStringUntil(char delimiter){auto end=bytes->find(delimiter,cursor);if(end==std::string::npos)end=bytes->size();String result=bytes->substr(cursor,end-cursor);cursor=std::min(end+1,bytes->size());return result;}
 size_t print(const String &value){if(writeFailure&&*writeFailure)return 0;bytes->append(value);return value.size();}void flush(){}void close(){}
};
namespace fs {class FS {public:bool failWrites=false;std::map<std::string,std::shared_ptr<std::string>> files;bool mkdir(const char*){return true;}bool exists(const char *path){return files.count(path)>0;}File open(const char *path,int mode=0){auto &entry=files[path];if(!entry&&mode==FILE_APPEND)entry=std::make_shared<std::string>();return File(entry,&failWrites);}};}
