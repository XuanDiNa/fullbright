#pragma once
#include <cstdint>
#include <array>
#include <span>
#include <string>
namespace fb {
struct Profile {
    std::array<char const*,2> hashes;
    std::uint32_t timestamp{},imageSize{},launcherSize{},textRva{},textSize{},hookRva{},hookEnd{};
    std::array<unsigned char,100> entry;
};
Profile const& ExactProfile();
std::string Sha256(std::span<unsigned char const>);
bool ValidateImage(std::span<unsigned char const>,Profile const&,bool loaded,std::string&);
bool ValidateHost(std::uintptr_t&,std::string&);
}
