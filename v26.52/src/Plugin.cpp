#include "FullBright/ConfigFile.h"
#include "FullBright/Native.h"
#include <windows.h>
#include <MinHook.h>
#include <atomic>
#include <fstream>
namespace {
HMODULE module{};HANDLE stopEvent{};std::atomic<int> status{-1};std::filesystem::path directory;
void Log(std::string const& message) noexcept {
    try {
        auto line="[FullBright] "+message+"\n";OutputDebugStringA(line.c_str());
        if(!directory.empty())std::ofstream(directory/L"FullBright.log",std::ios::app|std::ios::binary)<<GetTickCount64()<<' '<<line;
    }catch(...){}
}
int Init(){return static_cast<int>(MH_Initialize());}
int Create(void* t,void* h,void** o){return static_cast<int>(MH_CreateHook(t,h,o));}
int Enable(void* t){return static_cast<int>(MH_EnableHook(t));}
int Disable(void* t){return static_cast<int>(MH_DisableHook(t));}
int Remove(void* t){return static_cast<int>(MH_RemoveHook(t));}
int Uninit(){return static_cast<int>(MH_Uninitialize());}
bool ShortcutAllowed(){
    auto window=GetForegroundWindow();DWORD pid{};
    if(!window || !GetWindowThreadProcessId(window,&pid) || pid!=GetCurrentProcessId())return false;
    CURSORINFO cursor{};cursor.cbSize=sizeof cursor;
    return GetCursorInfo(&cursor) && !(cursor.flags&CURSOR_SHOWING);
}
void RunWorker(){
    wchar_t path[32768]{};if(!GetModuleFileNameW(module,path,32768))throw std::runtime_error("Cannot resolve plugin directory");
    directory=std::filesystem::path(path).parent_path();Log("START 0.1.0-test3-leftalt; exact 1.26.52.03; gamma-only; Windows x64");
    std::uintptr_t host{};std::string error;
    if(!fb::ValidateHost(host,error)){Log("INACTIVE: "+error);status.store(0);return;}
    fb::Config config;fb::ConfigFile file(directory/L"config.json");
    if(file.Poll(config,error)==fb::Reload::Rejected)Log("CONFIG REFUSED: "+error+"; packaged defaults retained");
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&RunWorker),&pinned))throw std::runtime_error("Cannot pin supported module");
    fb::Publish(config);
    if(!fb::Install(host,fb::ExactProfile(),{Init,Create,Enable,Disable,Remove,Uninit},error)){
        fb::Stop();Log("INACTIVE: "+error);status.store(0);return;
    }
    status.store(1);Log("READY enabled="+std::to_string(config.enabled)+" brightness="+std::to_string(config.brightness)+" key="+config.toggleKey);
    modmenu::binding::Tracker edge;auto nextPoll=GetTickCount64()+300;std::string lastError;bool observed=false;
    while(WaitForSingleObject(stopEvent,16)==WAIT_TIMEOUT){
        if(GetTickCount64()>=nextPoll){
            nextPoll=GetTickCount64()+300;auto result=file.Poll(config,error);
            if(result==fb::Reload::Applied){
                fb::Publish(config);edge.Reset();lastError.clear();
                Log("CONFIG APPLIED enabled="+std::to_string(config.enabled)+" brightness="+std::to_string(config.brightness)+" key="+config.toggleKey);
            }else if(result==fb::Reload::Rejected && lastError!=error){lastError=error;Log("CONFIG REFUSED: "+error);}
            if(!observed && fb::HookHits()>0){observed=true;Log("GAMMA HOOK OBSERVED; entry activity only, visual acceptance pending");}
        }
        bool allowed=config.virtualKey!=0 && ShortcutAllowed();
        auto sample=modmenu::binding::Sample(config.binding,[](unsigned k){return (GetAsyncKeyState(static_cast<int>(k))&0x8000)!=0;});
        if(edge.Update(config.binding,sample,allowed).pressed){
            if(file.Toggle(config,error)){fb::Publish(config);Log(config.enabled?"FullBright: ON":"FullBright: OFF");}
            else Log("TOGGLE REFUSED: "+error);
        }
    }
    fb::Stop();status.store(0);Log("STOPPED; restart required to reactivate");
}
DWORD WINAPI Worker(void* reference){
    try{RunWorker();}
    catch(std::exception const& e){fb::Stop();status.store(3);Log(std::string("FAILED: ")+e.what());}
    catch(...){fb::Stop();status.store(3);Log("FAILED: startup exception");}
    FreeLibraryAndExitThread(static_cast<HMODULE>(reference),0);
}
}
extern "C" __declspec(dllexport) int __cdecl FB_GetStatus(){return status.load();}
extern "C" __declspec(dllexport) char const* __cdecl FB_GetVersion(){return "0.1.0-test3-leftalt";}
extern "C" __declspec(dllexport) void __cdecl FB_Shutdown(){if(stopEvent)SetEvent(stopEvent);}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved){
    if(reason==DLL_PROCESS_ATTACH){
        module=instance;stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);HMODULE reference{};
        if(!stopEvent || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&Worker),&reference)){status.store(3);return TRUE;}
        auto thread=CreateThread(nullptr,0,Worker,reference,0,nullptr);
        if(thread)CloseHandle(thread);else{status.store(3);FreeLibrary(reference);}
    }else if(reason==DLL_PROCESS_DETACH && !reserved && stopEvent){CloseHandle(stopEvent);stopEvent=nullptr;}
    return TRUE;
}
