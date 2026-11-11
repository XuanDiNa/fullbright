#pragma once
#include "Core.h"
#include <filesystem>
namespace fb {
enum class Reload {Unchanged,Applied,Rejected};
class ConfigFile {
    std::filesystem::path path_;
    std::string observed_;
public:
    explicit ConfigFile(std::filesystem::path path):path_(std::move(path)){}
    Reload Poll(Config&,std::string& error);
    bool Toggle(Config&,std::string& error);
};
}
