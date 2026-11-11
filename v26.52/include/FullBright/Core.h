#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "sdk/ModMenuBinding.h"
namespace fb {
struct Config {
    bool enabled{true};
    float brightness{12.0f};
    std::string toggleKey{"NONE"};
    std::uint16_t virtualKey{};
    modmenu::binding::Binding binding{};
};
std::optional<std::uint16_t> ParseKey(std::string_view) noexcept;
std::vector<std::string> SupportedKeys();
std::optional<Config> ParseConfig(std::string_view,std::string& error);
std::string Serialize(Config const&);
float ResolveGamma(float original,bool enabled,float brightness) noexcept;
class KeyEdge {
    bool primed_{},previous_{};
public:
    bool Step(bool allowed,bool down) noexcept;
    void Reset() noexcept;
};
}
