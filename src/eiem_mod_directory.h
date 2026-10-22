#pragma once
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <vector>

// A Mod may contain independent child Mods. Discovery crosses that boundary;
// author-resource validation and change detection stop at it.
namespace EiemModDirectory {
inline bool Reparse(const std::filesystem::directory_entry& entry) {
  auto attributes=GetFileAttributesW(entry.path().c_str());
  return entry.is_symlink()||(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT));
}
inline bool Root(const std::filesystem::path& path) {
  return std::filesystem::is_regular_file(path/"mod.ini");
}
inline bool Hidden(const std::filesystem::path& path) {
  auto name=path.filename().u8string();return !name.empty()&&name.front()=='.';
}
inline std::vector<std::filesystem::path> Roots(const std::filesystem::path& directory) {
  std::vector<std::filesystem::path> result;
  if(!std::filesystem::exists(directory))return result;
  if(!std::filesystem::is_directory(directory))throw std::runtime_error("Mod path is not a directory");
  for(auto it=std::filesystem::recursive_directory_iterator(directory);it!=std::filesystem::recursive_directory_iterator();++it){
    if(Hidden(it->path())||Reparse(*it)){it.disable_recursion_pending();continue;}
    if(it->is_directory()&&Root(it->path()))result.push_back(std::filesystem::canonical(it->path()));
  }
  std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;
}
inline std::vector<std::filesystem::path> Files(const std::filesystem::path& root) {
  std::vector<std::filesystem::path> result;
  for(auto it=std::filesystem::recursive_directory_iterator(root);it!=std::filesystem::recursive_directory_iterator();++it){
    if(Reparse(*it))throw std::runtime_error("symlink/reparse resources forbidden");
    if(it->is_directory()&&Root(it->path())){it.disable_recursion_pending();continue;}
    if(it->is_regular_file())result.push_back(it->path());
  }
  return result;
}
inline bool Owns(const std::filesystem::path& root,const std::filesystem::path& file) {
  auto relative=file.lexically_relative(root);
  if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")return false;
  for(auto parent=file.parent_path();parent!=root;parent=parent.parent_path()){
    if(parent.empty()||parent==parent.parent_path())return false;
    if(Root(parent))return false;
  }
  return true;
}
}
