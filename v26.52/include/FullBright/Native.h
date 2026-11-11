#pragma once
#include "Core.h"
#include "Profile.h"
namespace fb {
using GammaFn=float(*)(void*);
struct HookApi {
    int (*initialize)();int (*create)(void*,void*,void**);int (*enable)(void*);
    int (*disable)(void*);int (*remove)(void*);int (*uninitialize)();
};
bool Install(std::uintptr_t,Profile const&,HookApi,std::string& error);
void Publish(Config const&) noexcept;
void Stop() noexcept;
std::uint64_t HookHits() noexcept;
}
