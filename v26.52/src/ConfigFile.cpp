#include "FullBright/ConfigFile.h"
#include <windows.h>
#include <atomic>
namespace fb {
namespace {
struct Handle {
    HANDLE h{INVALID_HANDLE_VALUE};explicit Handle(HANDLE value):h(value){}
    ~Handle(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
};
std::string Read(std::filesystem::path const& p){
    Handle f(CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    BY_HANDLE_FILE_INFORMATION info{};LARGE_INTEGER size{};
    if(f.h==INVALID_HANDLE_VALUE || !GetFileInformationByHandle(f.h,&info) || info.nNumberOfLinks!=1 ||
       (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) ||
       !GetFileSizeEx(f.h,&size) || size.QuadPart<=0 || size.QuadPart>65536)throw std::runtime_error("Config missing, unsafe, locked or too large");
    std::string bytes(static_cast<std::size_t>(size.QuadPart),'\0');DWORD n{};
    if(!ReadFile(f.h,bytes.data(),static_cast<DWORD>(bytes.size()),&n,nullptr) || n!=bytes.size())throw std::runtime_error("Cannot read config");
    return bytes;
}
}
Reload ConfigFile::Poll(Config& c,std::string& error){
    try {
        auto bytes=Read(path_);
        if(bytes==observed_){error.clear();return Reload::Unchanged;}
        auto next=ParseConfig(bytes,error);if(!next)return Reload::Rejected;
        c=std::move(*next);observed_=std::move(bytes);error.clear();return Reload::Applied;
    }catch(std::exception const& e){error=e.what();return Reload::Rejected;}
}
bool ConfigFile::Toggle(Config& c,std::string& error){
    std::filesystem::path temporary;
    try {
        // Optimistic conflict detection: never knowingly overwrite a newer ModMenu edit.
        auto before=Read(path_);
        if(before!=observed_)throw std::runtime_error("Config changed; reload before accepting shortcut");
        auto current=ParseConfig(before,error);if(!current)return false;
        if(current->enabled!=c.enabled || current->brightness!=c.brightness || current->toggleKey!=c.toggleKey)throw std::runtime_error("Runtime/config mismatch");
        current->enabled=!current->enabled;auto bytes=Serialize(*current);
        static std::atomic<unsigned long long> sequence{};
        temporary=path_;temporary+=L".fb-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++sequence)+L".tmp";
        {
            Handle f(CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr));DWORD n{};
            if(f.h==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create config transaction");
            if(!WriteFile(f.h,bytes.data(),static_cast<DWORD>(bytes.size()),&n,nullptr) || n!=bytes.size() || !FlushFileBuffers(f.h))throw std::runtime_error("Cannot write config transaction");
        }
        auto backup=path_;backup+=L".hotkey.bak";
        auto attributes=GetFileAttributesW(backup.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_READONLY)))throw std::runtime_error("Unsafe backup refused");
        if(Read(path_)!=before)throw std::runtime_error("Config changed during shortcut save");
        if(!ReplaceFileW(path_.c_str(),temporary.c_str(),backup.c_str(),0,nullptr,nullptr))throw std::runtime_error("Atomic config replacement refused");
        c=std::move(*current);observed_=std::move(bytes);error.clear();return true;
    }catch(std::exception const& e){
        if(!temporary.empty())DeleteFileW(temporary.c_str());error=e.what();return false;
    }
}
}
