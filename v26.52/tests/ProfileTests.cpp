#include "Test.h"
#include "FullBright/Profile.h"
#include <windows.h>
#include <cstring>
#include <fstream>
#include <vector>
int wmain(int argc,wchar_t** argv){
    using namespace fb;std::string error;auto p=ExactProfile();
    Check(Sha256({})=="E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855","standard empty hash vector");
    std::uintptr_t base{};Check(!ValidateHost(base,error) && base==0,"unsupported test process not admitted");
    for(int i=1;i<argc;++i){
        std::ifstream f(argv[i],std::ios::binary);std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(f),{}};
        Check(ValidateImage(bytes,p,false,error),"real supported disk image accepted");if(!error.empty())std::printf("%s\n",error.c_str());
        bytes.back()^=1;Check(!ValidateImage(bytes,p,false,error),"full image hash detects mutation outside entry");bytes.back()^=1;
        auto d=reinterpret_cast<IMAGE_DOS_HEADER*>(bytes.data());auto n=reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes.data()+d->e_lfanew);
        auto sections=IMAGE_FIRST_SECTION(n);std::vector<unsigned char> mapped(n->OptionalHeader.SizeOfImage);
        std::memcpy(mapped.data(),bytes.data(),n->OptionalHeader.SizeOfHeaders);
        for(unsigned k=0;k<n->FileHeader.NumberOfSections;++k)std::memcpy(mapped.data()+sections[k].VirtualAddress,bytes.data()+sections[k].PointerToRawData,sections[k].SizeOfRawData);
        Check(ValidateImage(mapped,p,true,error),"loaded layout accepted without executing game");
        for(unsigned k=0;k<100;++k){mapped[p.hookRva+k]^=1;Check(!ValidateImage(mapped,p,true,error),"every hook byte is guarded");mapped[p.hookRva+k]^=1;}
        mapped[0]=0;Check(!ValidateImage(mapped,p,true,error),"invalid loaded DOS header refused");mapped[0]='M';
        auto bad=p;bad.timestamp^=1;Check(!ValidateImage(mapped,bad,true,error),"wrong timestamp refused");
        bad=p;bad.hookRva=UINT32_MAX;Check(!ValidateImage(mapped,bad,true,error),"overflow hook refused");
        bad=p;bad.textSize=1;Check(!ValidateImage(mapped,bad,true,error),"text range mismatch refused");
        Check(!ValidateImage(std::span<unsigned char const>(bytes).first(30),p,false,error),"truncated PE refused");
    }
    return Finish("Profile");
}
