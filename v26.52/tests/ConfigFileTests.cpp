#include "Test.h"
#include "FullBright/ConfigFile.h"
#include <windows.h>
#include <fstream>
namespace fs=std::filesystem;
void Write(fs::path const& p,std::string const& s){std::ofstream(p,std::ios::binary|std::ios::trunc)<<s;}
std::string Read(fs::path const& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
    using namespace fb;
    auto root=fs::temp_directory_path()/("FullBrightFile-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));fs::create_directory(root);
    auto p=root/"config.json";Config c;std::string error;ConfigFile file(p);
    Check(file.Poll(c,error)==Reload::Rejected,"missing config does not write defaults");
    Write(p,"{\"enabled\":true,\"brightness\":7,\"toggleKey\":\"F6\"}");
    Check(file.Poll(c,error)==Reload::Applied && c.brightness==7 && c.virtualKey==117,"valid external edit applied");
    Check(file.Poll(c,error)==Reload::Unchanged,"unchanged config not reapplied");
    Check(file.Toggle(c,error) && !c.enabled && c.brightness==7 && c.virtualKey==117,"hotkey persists only enabled change");
    auto persisted=ParseConfig(Read(p),error);Check(persisted && !persisted->enabled && persisted->brightness==7,"toggle persisted to file");
    Check(Read(p.wstring()+L".hotkey.bak").find("\"enabled\":true")!=std::string::npos,"hotkey saves exact original backup");
    Write(p,"{broken");Check(file.Poll(c,error)==Reload::Rejected && !c.enabled && c.brightness==7,"malformed edit retains last good settings");
    Check(!file.Toggle(c,error) && Read(p)=="{broken","invalid file cannot be overwritten by key");
    Write(p,"{\"enabled\":true,\"brightness\":9,\"toggleKey\":\"NONE\"}");
    Check(!file.Toggle(c,error) && Read(p).find(":9")!=std::string::npos,"stale shortcut cannot overwrite newer ModMenu settings");
    Check(file.Poll(c,error)==Reload::Applied && c.brightness==9 && c.virtualKey==0,"valid edit recovers after malformed config");
    auto h=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Check(h!=INVALID_HANDLE_VALUE,"locked file fixture");
    Check(file.Poll(c,error)==Reload::Rejected && !file.Toggle(c,error),"locked config refuses reads and writes");
    if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
    SetFileAttributesW(p.c_str(),FILE_ATTRIBUTE_READONLY);
    Check(!file.Toggle(c,error) && c.enabled,"read-only persistence failure cannot change runtime enabled");
    SetFileAttributesW(p.c_str(),FILE_ATTRIBUTE_NORMAL);
    for(auto const& entry:fs::directory_iterator(root))fs::remove(entry.path());fs::remove(root);
    return Finish("ConfigFile");
}
