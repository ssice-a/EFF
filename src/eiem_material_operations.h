#pragma once
#include "eiem_resource_directory.h"
#include <optional>
namespace EiemMaterialOperations {
struct Edits {
 std::map<std::string,double> floats;
 std::map<std::string,int64_t> integers;
 std::map<std::string,std::array<double,4>> colors;
 std::map<std::string,std::array<double,2>> scales,offsets;
 std::map<std::string,bool> keywords;
 std::map<std::string,std::string> tags;
 std::optional<int32_t> queue;
 bool Empty()const{return floats.empty()&&integers.empty()&&colors.empty()&&scales.empty()&&offsets.empty()&&keywords.empty()&&tags.empty()&&!queue;}
};
}
