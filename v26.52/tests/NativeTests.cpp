#include "Test.h"
#include "FullBright/Native.h"
#include <cstring>
#include <vector>
namespace {
int created{},enabled{},disabled{},removed{},initResult{},createResult{},enableResult{};void* seen{};fb::GammaFn replacement{};int originals{};void* argument{};
float Original(void* p){++originals;argument=p;return 0.25f;}
int Init(){return initResult;}
int Create(void* target,void* hook,void** original){++created;seen=target;replacement=reinterpret_cast<fb::GammaFn>(hook);*original=reinterpret_cast<void*>(&Original);return createResult;}
int Enable(void*){++enabled;return enableResult;}
int Disable(void*){++disabled;return 0;}
int Remove(void*){++removed;return 0;}
int Uninit(){return 0;}
}
int main(){
    using namespace fb;HookApi api{Init,Create,Enable,Disable,Remove,Uninit};std::string error;auto p=ExactProfile();
    std::vector<unsigned char> image(p.hookEnd+1);std::memcpy(image.data()+p.hookRva,p.entry.data(),p.entry.size());
    auto base=reinterpret_cast<std::uintptr_t>(image.data());
    Check(!Install(0,p,api,error) && created==0,"zero host cannot install");
    image[p.hookRva]^=1;Check(!Install(base,p,api,error) && created==0,"live entry rechecked immediately before hook");image[p.hookRva]^=1;
    initResult=3;Check(!Install(base,p,api,error) && created==0,"initialization failure does not hook");initResult=0;
    createResult=8;Check(!Install(base,p,api,error) && enabled==0,"create failure does not enable");createResult=0;
    enableResult=9;Check(!Install(base,p,api,error) && removed==1,"enable failure removes never-active hook");enableResult=0;
    Check(Install(base,p,api,error) && seen==image.data()+p.hookRva && replacement,"exact target admitted");
    if(replacement){
        int sentinel=42;Config c;Publish(c);
        Check(replacement(&sentinel)==12 && originals==1 && argument==&sentinel,"actual detour preserves options and calls original exactly once");
        c.enabled=false;Publish(c);Check(replacement(&sentinel)==0.25f && originals==2,"disable immediately restores original result");
        c.enabled=true;c.brightness=25;Publish(c);Check(replacement(&sentinel)==25,"hot brightness applies without rehoking");
        Check(HookHits()==3,"diagnostic hit count does not invent gameplay proof");
        Check(!Install(base,p,api,error),"duplicate hook installation refused");
        Stop();Check(disabled==1 && replacement(&sentinel)==0.25f,"shutdown disables and in-flight hook passes through");
        Stop();Check(disabled==1 && removed==1,"shutdown idempotent and retains used trampoline");
    }
    return Finish("Native");
}
