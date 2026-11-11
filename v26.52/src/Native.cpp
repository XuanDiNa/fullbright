#include "FullBright/Native.h"
#include <windows.h>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstring>
namespace fb {
namespace {
GammaFn original{};HookApi hooks{};void* target{};bool installed{},used{};
std::atomic<std::uint64_t> state{},hits{};
float Gamma(void* options){
    auto vanilla=original(options);hits.fetch_add(1,std::memory_order_relaxed);
    auto value=state.load(std::memory_order_acquire);
    auto brightness=std::bit_cast<float>(static_cast<std::uint32_t>(value));
    return ResolveGamma(vanilla,(value>>32)!=0,brightness);
}
bool EntryMatches(std::uintptr_t base,Profile const& p){
    __try {
        if(!base || p.hookEnd<=p.hookRva || p.hookEnd-p.hookRva!=p.entry.size() || base>UINTPTR_MAX-p.hookEnd)return false;
        return std::memcmp(reinterpret_cast<void const*>(base+p.hookRva),p.entry.data(),p.entry.size())==0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
bool Install(std::uintptr_t base,Profile const& p,HookApi api,std::string& error){
    if(used || installed){error="Already installed or stopped; restart required";return false;}
    if(!api.initialize || !api.create || !api.enable || !api.disable || !api.remove || !api.uninitialize || !EntryMatches(base,p)){
        error="Invalid hook API or gamma entry conflict";return false;
    }
    int init=api.initialize();if(init!=0 && init!=1){error="Hook initialization failed";return false;}
    bool owner=init==0;auto at=reinterpret_cast<void*>(base+p.hookRva);void* trampoline{};
    if(api.create(at,reinterpret_cast<void*>(&Gamma),&trampoline)!=0 || !trampoline){
        if(owner)api.uninitialize();error="Gamma hook creation failed";return false;
    }
    original=reinterpret_cast<GammaFn>(trampoline);
    if(api.enable(at)!=0){
        api.remove(at);if(owner)api.uninitialize();original=nullptr;error="Gamma hook activation failed";return false;
    }
    hooks=api;target=at;installed=used=true;error.clear();return true;
}
void Publish(Config const& c) noexcept {
    bool valid=c.enabled && std::isfinite(c.brightness) && c.brightness>=1 && c.brightness<=25;
    state.store(static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(c.brightness))|(static_cast<std::uint64_t>(valid)<<32),std::memory_order_release);
}
void Stop() noexcept {
    state.store(0,std::memory_order_release);
    if(installed){hooks.disable(target);installed=false;}
    // A supported host pins the DLL. Retain a used trampoline until process exit:
    // an in-flight original call must never jump into reclaimed trampoline memory.
}
std::uint64_t HookHits() noexcept {return hits.load(std::memory_order_relaxed);}
}
