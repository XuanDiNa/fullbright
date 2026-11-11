#include "FullBright/Core.h"
#include <json.hpp>
#include <charconv>
#include <cmath>
#include <set>
namespace fb {
namespace {
struct KeyName {std::string_view name;std::uint16_t vk;};
constexpr KeyName names[]{
    {"NONE",0},{"LCTRL",162},{"RCTRL",163},{"LSHIFT",160},{"RSHIFT",161},{"LALT",164},{"RALT",165},
    {"SPACE",32},{"INSERT",45},{"DELETE",46},{"HOME",36},{"END",35},{"PAGEUP",33},{"PAGEDOWN",34},{"PAUSE",19}};
}
std::optional<std::uint16_t> ParseKey(std::string_view text) noexcept {
    for(auto [name,vk]:names)if(text==name)return vk;
    if(text.size()==1 && ((text[0]>='A' && text[0]<='Z') || (text[0]>='0' && text[0]<='9')))return static_cast<std::uint16_t>(text[0]);
    if(text.size()>=2 && text.size()<=3 && text[0]=='F' && text[1]!='0'){
        unsigned n{};auto r=std::from_chars(text.data()+1,text.data()+text.size(),n);
        if(r.ec==std::errc{} && r.ptr==text.data()+text.size() && n>=1 && n<=24)return static_cast<std::uint16_t>(111+n);
    }
    return {};
}
std::vector<std::string> SupportedKeys(){
    std::vector<std::string> result;for(auto [name,vk]:names){(void)vk;result.emplace_back(name);}
    for(char c='A';c<='Z';++c)result.emplace_back(1,c);
    for(char c='0';c<='9';++c)result.emplace_back(1,c);
    for(int n=1;n<=24;++n)result.push_back("F"+std::to_string(n));return result;
}
std::optional<Config> ParseConfig(std::string_view text,std::string& error){
    try {
        if(text.size()>65536)throw std::runtime_error("Config exceeds 64 KiB");
        std::set<std::string> keys;
        auto cb=[&](int depth,nlohmann::json::parse_event_t event,nlohmann::json& v){
            if(depth>2)throw std::runtime_error("Config nesting refused");
            if(event==nlohmann::json::parse_event_t::key && !keys.insert(v.get<std::string>()).second)throw std::runtime_error("Duplicate member");return true;
        };
        auto j=nlohmann::json::parse(text,cb);if(!j.is_object())throw std::runtime_error("Config must be an object");
        Config next;
        for(auto const& [key,v]:j.items()){
            if(key=="enabled"){
                if(!v.is_boolean())throw std::runtime_error("enabled must be boolean");next.enabled=v.get<bool>();
            }else if(key=="brightness"){
                if(!v.is_number())throw std::runtime_error("brightness must be numeric");
                auto n=v.get<double>();if(!std::isfinite(n) || n<1 || n>25)throw std::runtime_error("brightness must be 1..25");next.brightness=static_cast<float>(n);
            }else if(key=="toggleKey"){
                if(!v.is_string())throw std::runtime_error("toggleKey must be a canonical token");
                next.toggleKey=v.get<std::string>();auto b=modmenu::binding::ParseInput(next.toggleKey);if(!b)throw std::runtime_error("Unsupported toggleKey");next.binding=*b;next.virtualKey=static_cast<std::uint16_t>(b->key);
            }else throw std::runtime_error("Unknown config member");
        }
        error.clear();return next;
    }catch(std::exception const& e){error=e.what();return {};}
}
std::string Serialize(Config const& c){
    nlohmann::json j{{"enabled",c.enabled},{"toggleKey",c.toggleKey}};
    if(std::isfinite(c.brightness) && std::floor(c.brightness)==c.brightness)j["brightness"]=static_cast<int>(c.brightness);
    else j["brightness"]=c.brightness;
    return j.dump(2)+"\n";
}
float ResolveGamma(float original,bool enabled,float brightness) noexcept {
    if(!enabled || !std::isfinite(original) || !std::isfinite(brightness) || brightness<1 || brightness>25)return original;
    return brightness;
}
bool KeyEdge::Step(bool allowed,bool down) noexcept {
    if(!allowed){Reset();return false;}
    if(!primed_){primed_=true;previous_=down;return false;}
    bool pressed=down && !previous_;previous_=down;return pressed;
}
void KeyEdge::Reset() noexcept {primed_=previous_=false;}
}
