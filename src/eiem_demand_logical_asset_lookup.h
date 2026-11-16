#pragma once
#include <string>

namespace EiemLogicalAssetLookup {
template<class LookupByPath,class LookupByPathAndAsset>
bool Find(const std::string& path,LookupByPath&& byPath,LookupByPathAndAsset&& byPathAndAsset){
 const bool subasset=(path.size()>=4&&path.compare(path.size()-4,4,".fbx")==0)||
   (path.size()>=6&&path.compare(path.size()-6,6,".asset")==0);
 if(subasset)return byPathAndAsset()||byPath();
 return byPath()||byPathAndAsset();
}
} // namespace EiemLogicalAssetLookup
