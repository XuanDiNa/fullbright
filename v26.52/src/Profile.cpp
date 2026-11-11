#include "FullBright/Profile.h"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <cstring>
#include <vector>
namespace fb {
#include "NativeProfile.inl"
Profile const& ExactProfile(){static Profile p=MakeProfile();return p;}
std::string Sha256(std::span<unsigned char const> bytes){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};std::string result;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
    DWORD size{},used{};
    if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof size,&used,0)>=0){
        std::vector<unsigned char> object(size);std::array<unsigned char,32> digest{};
        if(BCryptCreateHash(algorithm,&hash,object.data(),size,nullptr,0,0)>=0){
            bool ok=true;
            for(std::size_t i=0;i<bytes.size();){
                auto n=static_cast<ULONG>(std::min<std::size_t>(bytes.size()-i,1u<<20));
                if(BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()+i),n,0)<0){ok=false;break;}i+=n;
            }
            if(ok && BCryptFinishHash(hash,digest.data(),32,0)>=0){
                constexpr char hex[]="0123456789ABCDEF";for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}
            }
            BCryptDestroyHash(hash);
        }
    }
    BCryptCloseAlgorithmProvider(algorithm,0);return result;
}
namespace {
bool Fits(std::size_t start,std::size_t n,std::size_t total){return start<=total && n<=total-start;}
bool Loaded(std::uintptr_t base,std::string& error){
    __try {
        auto d=reinterpret_cast<IMAGE_DOS_HEADER const*>(base);
        if(d->e_magic!=IMAGE_DOS_SIGNATURE || d->e_lfanew<=0 || d->e_lfanew>0x100000){error="Invalid loaded header";return false;}
        auto n=reinterpret_cast<IMAGE_NT_HEADERS64 const*>(base+d->e_lfanew);
        auto const& p=ExactProfile();
        if(n->OptionalHeader.SizeOfImage!=p.imageSize && n->OptionalHeader.SizeOfImage!=p.launcherSize){error="Unsupported loaded size";return false;}
        return ValidateImage({reinterpret_cast<unsigned char const*>(base),n->OptionalHeader.SizeOfImage},p,true,error);
    }__except(EXCEPTION_EXECUTE_HANDLER){error="Unreadable loaded image";return false;}
}
}
bool ValidateImage(std::span<unsigned char const> bytes,Profile const& p,bool loaded,std::string& error){
    auto reject=[&](char const* s){error=s;return false;};
    if(bytes.size()<sizeof(IMAGE_DOS_HEADER))return reject("Truncated DOS header");
    auto d=reinterpret_cast<IMAGE_DOS_HEADER const*>(bytes.data());
    if(d->e_magic!=IMAGE_DOS_SIGNATURE || d->e_lfanew<0 || !Fits(d->e_lfanew,sizeof(IMAGE_NT_HEADERS64),bytes.size()))return reject("Invalid PE header");
    auto n=reinterpret_cast<IMAGE_NT_HEADERS64 const*>(bytes.data()+d->e_lfanew);
    if(n->Signature!=IMAGE_NT_SIGNATURE || n->FileHeader.Machine!=0x8664 || n->OptionalHeader.Magic!=0x20b ||
       n->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64) || n->FileHeader.TimeDateStamp!=p.timestamp ||
       (n->OptionalHeader.SizeOfImage!=p.imageSize && n->OptionalHeader.SizeOfImage!=p.launcherSize) ||
       n->FileHeader.NumberOfSections==0 || n->FileHeader.NumberOfSections>96)return reject("Unsupported AMD64 image metadata");
    auto table=static_cast<std::size_t>(d->e_lfanew)+sizeof(IMAGE_NT_HEADERS64);
    if(!Fits(table,n->FileHeader.NumberOfSections*sizeof(IMAGE_SECTION_HEADER),bytes.size()) ||
       (loaded && bytes.size()!=n->OptionalHeader.SizeOfImage))return reject("Invalid image/section bounds");
    auto sections=reinterpret_cast<IMAGE_SECTION_HEADER const*>(bytes.data()+table);IMAGE_SECTION_HEADER const* text{};
    for(unsigned i=0;i<n->FileHeader.NumberOfSections;++i){
        auto const& s=sections[i];
        if(!Fits(s.VirtualAddress,std::max(s.Misc.VirtualSize,s.SizeOfRawData),n->OptionalHeader.SizeOfImage) ||
           (!loaded && !Fits(s.PointerToRawData,s.SizeOfRawData,bytes.size())))return reject("Invalid section extent");
        if(std::memcmp(s.Name,".text\0\0\0",8)==0){if(text)return reject("Duplicate text section");text=&s;}
    }
    if(!text || !(text->Characteristics&IMAGE_SCN_MEM_EXECUTE) || text->VirtualAddress!=p.textRva || text->Misc.VirtualSize!=p.textSize ||
       p.hookEnd<=p.hookRva || p.hookEnd-p.hookRva!=p.entry.size() || p.hookRva<p.textRva ||
       !Fits(p.hookRva-p.textRva,p.entry.size(),p.textSize) || !Fits(p.hookRva-p.textRva,p.entry.size(),text->SizeOfRawData))return reject("Unbounded gamma target/text section");
    auto offset=loaded?static_cast<std::size_t>(p.hookRva):static_cast<std::size_t>(text->PointerToRawData)+p.hookRva-p.textRva;
    if(!Fits(offset,p.entry.size(),bytes.size()) || std::memcmp(bytes.data()+offset,p.entry.data(),p.entry.size()))return reject("Gamma entry changed or already hooked");
    if(!loaded){auto digest=Sha256(bytes);if(digest.empty() || (digest!=p.hashes[0] && digest!=p.hashes[1]))return reject("Unrecognized complete image SHA256");}
    error.clear();return true;
}
bool ValidateHost(std::uintptr_t& base,std::string& error){
    base=0;wchar_t path[32768]{};
    if(!GetModuleFileNameW(nullptr,path,32768)){error="Cannot locate host";return false;}
    auto file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){error="Cannot read host image";return false;}
    LARGE_INTEGER size{};HANDLE mapping{};void* view{};bool accepted=false;
    if(GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<0x40000000){
        mapping=CreateFileMappingW(file,nullptr,PAGE_READONLY,0,0,nullptr);
        if(mapping)view=MapViewOfFile(mapping,FILE_MAP_READ,0,0,0);
        if(view)accepted=ValidateImage({static_cast<unsigned char const*>(view),static_cast<std::size_t>(size.QuadPart)},ExactProfile(),false,error);
    }
    if(view)UnmapViewOfFile(view);if(mapping)CloseHandle(mapping);CloseHandle(file);
    auto candidate=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(accepted && Loaded(candidate,error)){base=candidate;return true;}
    if(error.empty())error="Cannot map supported host";return false;
}
}
