#include "Test.h"
#include "FullBright/Core.h"
#include <cmath>
#include <limits>
int main(){
    using namespace fb;std::string error;
    Check(ParseConfig(R"({"toggleKey":"CTRL+SHIFT+K"})",error).has_value(),"public chord accepted by real config parser");
    // Catches missing defaults, weak JSON validation and loss of brightness/key on save.
    auto defaults=ParseConfig("{}",error);
    Check(defaults && defaults->enabled && defaults->brightness==12 && defaults->virtualKey==0,"safe unbound packaged defaults");
    for(auto text:{"[]","null","{\"enabled\":1}","{\"brightness\":0}","{\"brightness\":26}","{\"brightness\":\"12\"}","{\"toggleKey\":\"ESC\"}","{\"toggleKey\":\"F25\"}","{\"enabled\":true,\"enabled\":false}","{\"hookTargetRva\":123}","{\"brightness\":1e1000}","{broken"})
        Check(!ParseConfig(text,error),"invalid config refused");
    for(auto brightness:{1.0f,12.0f,25.0f}){
        Config c;c.brightness=brightness;c.enabled=false;c.toggleKey="F6";c.virtualKey=117;
        auto roundtrip=ParseConfig(Serialize(c),error);
        Check(roundtrip && !roundtrip->enabled && roundtrip->brightness==brightness && roundtrip->virtualKey==117,"roundtrip preserves enabled brightness and binding");
    }
    Check(ParseConfig("{\"brightness\":12.5}",error).has_value(),"JSON decimal brightness supported");
    auto none=ParseKey("NONE");Check(none && *none==0,"NONE is zero not fallback");
    Check(ParseKey("LCTRL")==162 && ParseKey("F24")==135 && ParseKey("A")==65,"canonical key tokens");
    for(auto key:{"","F0","F01","F25","CTRL+F6","esc","NONE ","MB1"})Check(!ParseKey(key),"bad key has no fallback");
    Check(SupportedKeys().size()>60,"usable key allowlist");
    for(auto const& token:SupportedKeys())Check(ParseKey(token).has_value(),"every offered key parses");
    // Catches override when disabled and incorrect clamp/invalid result handling.
    Config c;
    for(float vanilla:{0.0f,0.5f,1.0f})Check(ResolveGamma(vanilla,c.enabled,c.brightness)==12,"enabled gamma override including zero brightness");
    c.enabled=false;Check(ResolveGamma(0.3f,c.enabled,c.brightness)==0.3f,"disabled preserves vanilla exactly");
    c.enabled=true;c.brightness=25;Check(ResolveGamma(0.5f,c.enabled,c.brightness)==25,"maximum gamma works");
    c.brightness=0;Check(ResolveGamma(0.5f,c.enabled,c.brightness)==0.5f,"invalid runtime config fails closed");
    c.brightness=12;Check(std::isnan(ResolveGamma(std::numeric_limits<float>::quiet_NaN(),c.enabled,c.brightness)),"nonfinite original remains untouched");
    // Catches key repeat, focus return phantom edges and bind-capture toggle.
    KeyEdge edge;Check(!edge.Step(true,false),"initial release primes");
    Check(edge.Step(true,true),"fresh press toggles");
    for(int n=0;n<50;++n)Check(!edge.Step(true,true),"holding cannot repeat");
    Check(!edge.Step(true,false) && edge.Step(true,true),"second fresh press toggles");
    Check(!edge.Step(false,true) && !edge.Step(true,true),"UI and focus return ignore held key");
    Check(!edge.Step(true,false) && edge.Step(true,true),"release re-arms after UI");
    edge.Reset();Check(!edge.Step(true,true),"new binding held on apply does not toggle");
    return Finish("Core");
}
