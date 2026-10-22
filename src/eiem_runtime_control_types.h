#pragma once
// Included in EiemDirectory, after Package and the resource block helpers.
struct MaterialTrack {std::string material,property,kind;unsigned component=0;};
struct EffectAttachment {std::string id,label,part,bone,variable;bool autoplay=true;std::array<float,3> position{0,0,0},scale{1,1,1};std::array<float,4> rotation{0,0,0,1};std::shared_ptr<const EiemEffectNative::Manifest> native;std::string targetPrefab,rendererPath,targetPath,targetAsset;std::vector<std::string> targetBones;};
struct ControlReader {
 const Resource& resource;size_t p,end;
 uint64_t N(size_t n=4){Require(p<=end&&n<=end-p,"runtime control truncated");auto value=Le(resource.bytes,p,n);p+=n;return value;}
 std::vector<std::string> Strings(){auto count=N();Require(count<=(end-p)/2,"runtime control strings extent");std::vector<std::string> result;for(uint64_t i=0;i<count;++i){auto size=N(2);Require(size<=end-p,"runtime control string extent");result.push_back(Text(resource.bytes.data()+p,size_t(size)));p+=size_t(size);}return result;}
};
inline ShapeAnimation DecodeMaterialAnimation(const Resource& resource,std::string& owner,std::map<std::string,MaterialTrack>& targets){
 Require(resource.kind==9&&resource.blocks.size()==1&&resource.blocks.count("@material-animation"),"material animation layout");auto block=resource.blocks.at("@material-animation");ControlReader r{resource,block.offset,block.offset+block.size};
 auto count=r.N();Require(count&&count<=(r.end-r.p)/15,"material animation target extent");std::set<std::tuple<std::string,std::string,unsigned>> unique;
 for(uint64_t i=0;i<count;++i){auto names=r.Strings();Require(names.size()==4&&EiemVariableName(names[0])&&Identifier(names[1])&&!names[2].empty()&&names[2][0]=='_'&&names[2].find_first_of("\t\n")==names[2].npos,"material animation target identity");MaterialTrack t{names[1],names[2],names[3],unsigned(r.N(1))};
  Require((t.kind=="FLOAT"||t.kind=="COLOR"||t.kind=="VECTOR")&&t.component<(t.kind=="FLOAT"?1u:4u)&&unique.emplace(t.material,t.property,t.component).second&&targets.emplace(names[0],t).second,"material animation duplicate/type");}
 auto sampled=resource;sampled.kind=8;sampled.blocks.clear();sampled.blocks.emplace("@shape-animation",Block{r.p,r.end-r.p,{}});auto decoded=DecodeShapeAnimation(sampled);owner=decoded.first;
 Require(decoded.second.tracks.size()==targets.size(),"material animation sample coverage");for(const auto& row:targets)Require(decoded.second.tracks.count(row.first),"material animation sample absent");return decoded.second;
}
inline std::pair<std::string,EffectAttachment> DecodeEffectAttachment(const Resource& resource,std::shared_ptr<const Resource> backing){
 Require(resource.kind==10&&resource.blocks.count("@effect-attachment")&&resource.blocks.size()==1+resource.blocks.count("@effect-target"),"effect attachment layout");auto block=resource.blocks.at("@effect-attachment");ControlReader r{resource,block.offset,block.offset+block.size};
 auto names=r.Strings();Require(names.size()==6&&Identifier(names[0])&&Identifier(names[1])&&(names[4].empty()||Identifier(names[4])),"effect attachment identity");EffectAttachment e;e.id=names[1];e.label=names[2];e.part=names[4];e.bone=names[5];e.variable="$fx_"+e.id;auto autoplay=r.N(1);Require(autoplay<=1,"effect autoplay domain");e.autoplay=autoplay!=0;
 if(resource.blocks.count("@effect-target")){auto b=resource.blocks.at("@effect-target");ControlReader target{resource,b.offset,b.offset+b.size};auto fields=target.Strings();Require(target.p==target.end&&fields.size()>4&&Begins(fields[0],"assets/")&&fields[0].find("..") == fields[0].npos&&!fields[1].empty()&&Begins(fields[2],"assets/")&&!fields[3].empty(),"effect target proof invalid");e.targetPrefab=fields[0];e.rendererPath=fields[1];e.targetPath=fields[2];e.targetAsset=fields[3];e.targetBones.assign(fields.begin()+4,fields.end());}
 Require(!e.part.empty()||!e.targetPrefab.empty(),"effect target absent");
 Require(e.bone.empty()||(e.bone.front()!='/'&&e.bone.back()!='/'&&e.bone.find('\\')==e.bone.npos&&e.bone.find("//")==e.bone.npos&&e.bone.find("..")==e.bone.npos),"effect bone path invalid");
 auto scalar=[&](){uint32_t bits=uint32_t(r.N());float v;memcpy(&v,&bits,4);Require(std::isfinite(v),"effect transform nonfinite");return v;};for(auto& v:e.position)v=scalar();for(auto& v:e.rotation)v=scalar();for(auto& v:e.scale)v=scalar();
 double norm=0;for(auto v:e.rotation)norm+=double(v)*v;Require(std::abs(norm-1)<1e-3,"effect rotation must be normalized");
 auto native=r.Strings();Require(native.size()==3&&native[0].size()==64&&native[0].find_first_not_of("0123456789ABCDEF")==native[0].npos&&Begins(native[1],"assets/")&&native[1].find("..")==native[1].npos&&!native[2].empty(),"effect native identity");
 auto m=std::make_shared<EiemEffectNative::Manifest>();m->key=native[0];m->prefab=native[1];m->rootName=native[2];m->root=size_t(r.N());m->particles=size_t(r.N());auto count=r.N();Require(count&&count<=(r.end-r.p)/16&&m->root<count,"effect native bundle extent");
 for(uint64_t i=0;i<count;++i){auto bnames=r.Strings();Require(bnames.size()==2,"effect Bundle identity");EiemEffectNative::Bundle b;b.file=bnames[0];b.hash=bnames[1];auto n=r.N();Require(n<=count,"effect native dependency extent");std::set<size_t> deps;
  for(uint64_t j=0;j<n;++j){auto d=size_t(r.N());Require(d<count&&d!=i&&deps.insert(d).second,"effect native dependency index");b.dependencies.push_back(d);}auto size=r.N(8);Require(size<=r.end-r.p&&size>=8&&backing&&backing.get()==&resource,"effect native backing absent");
  b.backing=backing;b.data=resource.bytes.data()+r.p;b.size=size_t(size);r.p+=b.size;
  auto hash=Sha(b.data,b.size);Require(Hex(hash.data(),hash.size())==b.hash&&EiemEffectNative::InterchangeArchive(b.data,b.size),"effect archive bytes/digest differ");m->bundles.push_back(std::move(b));}
 Require(r.p==r.end,"effect attachment trailing bytes");e.native=std::move(m);return {names[0],std::move(e)};
}
