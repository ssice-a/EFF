#pragma once
#include <string>

namespace EiemLogicalAssetLookup {
template<class LookupByPath,class LookupByPathAndAsset>
bool Find(const std::string& path,LookupByPath&& byPath,LookupByPathAndAsset&& byPathAndAsset){
 const bool fbx=path.size()>=4&&path.compare(path.size()-4,4,".fbx")==0;
 if(fbx)return byPathAndAsset()||byPath();
 return byPath()||byPathAndAsset();
}
} // namespace EiemLogicalAssetLookup
