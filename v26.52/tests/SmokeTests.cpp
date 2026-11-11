#include "Test.h"
#include <windows.h>
#include <filesystem>
#include <string>
int main(){
    wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);
    auto module=LoadLibraryW((std::filesystem::path(exe).parent_path()/L"FullBright.dll").c_str());
    Check(module!=nullptr,"DLL loads into unsupported test process");
    if(module){
        auto status=reinterpret_cast<int(*)()>(GetProcAddress(module,"FB_GetStatus"));auto version=reinterpret_cast<char const*(*)()>(GetProcAddress(module,"FB_GetVersion"));auto stop=reinterpret_cast<void(*)()>(GetProcAddress(module,"FB_Shutdown"));
        Check(status && version && stop,"diagnostic exports available");
        if(status && version && stop){
            for(unsigned n=0;n<150 && status()==-1;++n)Sleep(20);
            Check(status()==0,"unsupported host never activates native hooks");Check(std::string(version())=="0.1.0-test3-leftalt","binary version matches metadata");
            stop();stop();Check(status()==0,"inactive shutdown idempotent");
        }
        FreeLibrary(module);
    }
    return Finish("Smoke");
}
