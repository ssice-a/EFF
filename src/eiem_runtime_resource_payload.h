#pragma once
#include "eiem_material_operations.h"
#include <cfloat>
#include <cstring>

// Direct readers for the self-contained author fields in EFFRES2. They never
// import a game TypeTree, source Bundle, consumer PFB, or compiled Mod store.
// Views borrow the verified Resource, so decoding does not copy vertex/pixels.
namespace EiemRuntimePayload {
using namespace EiemDirectory;
struct Span { const unsigned char* data=nullptr; size_t size=0; };
struct Cursor {
 Span bytes; size_t pos=0;
 uint64_t N(size_t width=4){auto n=Le(bytes.data,bytes.size,pos,width);pos+=width;return n;}
 Span Take(size_t count){Require(pos<=bytes.size&&count<=bytes.size-pos,"author field truncated");Span s{bytes.data+pos,count};pos+=count;return s;}
 void Align(){auto end=AlignSize(pos,4);auto pad=Take(end-pos);for(size_t i=0;i<pad.size;++i)Require(!pad.data[i],"nonzero author field alignment");}
 size_t Count(size_t width){auto n=N();Require(width&&n<=(bytes.size-pos)/width,"author array exceeds field");return size_t(n);}
 float F(){auto bits=uint32_t(N());float v;memcpy(&v,&bits,4);Require(std::isfinite(v),"nonfinite author float");return v;}
 std::string S(){auto n=N();auto s=Take(size_t(n));auto value=Text(s.data,s.size);Align();return value;}
 void End(){Require(pos==bytes.size,"unknown author field suffix");}
};
inline Span Field(const Resource& r,const char* name){auto i=r.blocks.find(name);Require(i!=r.blocks.end(),std::string("author field absent: ")+name);return {ResourceData(r)+i->second.offset,i->second.size};}
inline Cursor ReadField(const Resource& r,const char* name){return {Field(r,name)};}
inline Span ArrayField(const Resource& r,const char* name,size_t width){auto c=ReadField(r,name);auto n=c.Count(width);auto s=c.Take(MultiplySize(n,width));c.Align();c.End();return s;}
inline void ZeroField(const Resource& r,const char* name){auto b=Field(r,name);Require(std::all_of(b.data,b.data+b.size,[](auto v){return !v;}),std::string("unsupported nonempty author field: ")+name);}
struct VertexChannel {unsigned stream=0,offset=0,format=0,dimension=0;};
struct SparseVertex {std::array<float,3> position{},normal{},tangent{};uint32_t index=0;};
struct ShapeFrame {unsigned first=0,count=0;bool normals=false,tangents=false;float weight=0;};
struct ShapeChannel {std::string name;unsigned first=0,count=0;};
struct Mesh {
 unsigned vertices=0,indices=0,indexFormat=0;
 std::array<VertexChannel,14> channels{};
 std::array<size_t,4> stride{},start{};
 Span vertexData,indexData,bindPoseData,boneBounds;
 std::array<float,6> bounds{};
 std::vector<std::string> bones,shapeNames;
 std::vector<SparseVertex> shapeVertices;
 std::vector<ShapeFrame> frames;
 std::vector<ShapeChannel> shapes;
 bool packedFrame=false;
};
inline Mesh DecodeMesh(const Resource& r){
 Require(r.kind==1&&ResourceSize(r)>=80&&ResourceData(r)[6]=='2',"author payload is not an EFFRES2 Mesh");Mesh m;
 auto g=ReadField(r,"@geometry");m.vertices=unsigned(g.N());m.indices=unsigned(g.N());g.End();
 Require(m.vertices&&m.indices&&m.vertices<=INT_MAX&&m.indices<=INT_MAX&&m.indices%3==0,"invalid native Mesh counts");
 m.bones=Strings(r,"@bones");m.shapeNames=Strings(r,"@shapes");
 auto v=ReadField(r,"m_VertexData");Require(v.N()==m.vertices&&v.N()==14,"author Mesh vertex/channel counts differ");
 constexpr size_t widths[]={4,2,1,1,2,2,1,1,2,2,4,4};bool occupied[4][256]{};
 for(auto& channel:m.channels){channel.stream=unsigned(v.N(1));channel.offset=unsigned(v.N(1));channel.format=unsigned(v.N(1));channel.dimension=unsigned(v.N(1));auto dim=channel.dimension&15;
  if(!dim)continue;Require(dim<=4&&channel.stream<4&&channel.format<12,"unsupported author vertex channel");auto end=channel.offset+dim*widths[channel.format];Require(end<=256,"author vertex channel extent");
  for(size_t p=channel.offset;p<end;++p){Require(!occupied[channel.stream][p],"overlapping author vertex channels");occupied[channel.stream][p]=true;}
  m.stride[channel.stream]=std::max(m.stride[channel.stream],end);
 }
 auto vertexBytes=v.N();m.vertexData=v.Take(size_t(vertexBytes));v.Align();v.End();size_t extent=0;
 for(size_t stream=0;stream<4;++stream){m.start[stream]=extent;extent=AlignSize(AddSize(extent,MultiplySize(m.vertices,m.stride[stream])),16);}
 Require(extent==m.vertexData.size&&m.channels[0].dimension==3&&m.channels[0].format==0,"author vertex storage differs");
 m.packedFrame=m.channels[1].dimension==0x31;
 Require((m.packedFrame&&!(m.channels[2].dimension&15))||(m.channels[1].dimension==3&&m.channels[2].dimension==4),"unsupported author normal/tangent storage");
 for(size_t index=0;index<m.channels.size();++index){const auto& c=m.channels[index];auto dim=c.dimension&15;if(!dim||c.format!=0||(index==1&&m.packedFrame))continue;for(size_t vertex=0;vertex<m.vertices;++vertex)for(size_t axis=0;axis<dim;++axis){float value=0;memcpy(&value,m.vertexData.data+m.start[c.stream]+vertex*m.stride[c.stream]+c.offset+axis*4,4);Require(std::isfinite(value),"nonfinite author vertex attribute");}}
 auto format=ReadField(r,"m_IndexFormat");m.indexFormat=unsigned(format.N());format.End();Require(m.indexFormat<=1,"unsupported Mesh index format");
 m.indexData=ArrayField(r,"m_IndexBuffer",1);auto iw=m.indexFormat?4u:2u;Require(m.indexData.size==MultiplySize(m.indices,iw),"author index byte count differs");
 for(size_t p=0;p<m.indexData.size;p+=iw)Require(Le(m.indexData.data,m.indexData.size,p,iw)<m.vertices,"author index outside Mesh");
 auto sub=ReadField(r,"m_SubMeshes");Require(sub.N()==1&&sub.N()==0&&sub.N()==m.indices&&sub.N()==0&&sub.N()==0&&sub.N()==0&&sub.N()==m.vertices,"author Mesh is not one complete triangle submesh");
 for(int i=0;i<6;++i)(void)sub.F();sub.End();
 auto bounds=ReadField(r,"m_LocalAABB");for(auto& f:m.bounds)f=bounds.F();bounds.End();for(size_t i=3;i<6;++i)Require(m.bounds[i]>=0,"negative Mesh extent");
 m.bindPoseData=ArrayField(r,"m_BindPose",64);m.boneBounds=ArrayField(r,"m_BonesAABB",24);
 auto hashes=ArrayField(r,"m_BoneNameHashes",4);Require(m.bindPoseData.size/64==m.bones.size()&&hashes.size/4==m.bones.size()&&m.boneBounds.size/24==m.bones.size(),"author bone metadata count differs");
 auto bpv=ReadField(r,"m_BonesPerVertex");Require(bpv.N()==(m.bones.empty()?0u:4u),"unsupported variable author bone weights");bpv.End();
 if(!m.bones.empty()){
  Require(m.channels[12].format==4&&m.channels[12].dimension==4&&m.channels[13].format==6&&m.channels[13].dimension==4,"unsupported author skin channel encoding");
  const auto& w=m.channels[12];const auto& b=m.channels[13];
  for(size_t i=0;i<m.vertices;++i){unsigned sum=0;for(size_t j=0;j<4;++j){auto weight=Le(m.vertexData.data,m.vertexData.size,m.start[w.stream]+i*m.stride[w.stream]+w.offset+j*2,2);sum+=unsigned(weight);auto bone=Le(m.vertexData.data,m.vertexData.size,m.start[b.stream]+i*m.stride[b.stream]+b.offset+j,1);Require(!weight||bone<m.bones.size(),"author skin index outside palette");}Require(sum>=65532&&sum<=65538,"author skin weights are not normalized");}
 }else Require(!(m.channels[12].dimension&15)&&!(m.channels[13].dimension&15),"unskinned author contains skin channels");
 ZeroField(r,"m_MeshCompression");ZeroField(r,"m_VariableBoneCountWeights");ZeroField(r,"m_CompressedMesh");ZeroField(r,"m_StreamData");
 auto s=ReadField(r,"m_Shapes");auto count=s.Count(40);m.shapeVertices.resize(count);
 for(auto& vertex:m.shapeVertices){for(auto* xyz:{&vertex.position,&vertex.normal,&vertex.tangent})for(auto& f:*xyz)f=s.F();vertex.index=unsigned(s.N());Require(vertex.index<m.vertices,"shape vertex outside Mesh");}
 count=s.Count(12);m.frames.resize(count);for(auto& f:m.frames){f.first=unsigned(s.N());f.count=unsigned(s.N());auto normal=s.N(1),tangent=s.N(1);Require(normal<=1&&tangent<=1,"shape flags differ");f.normals=normal!=0;f.tangents=tangent!=0;s.Align();Require(f.first<=m.shapeVertices.size()&&f.count<=m.shapeVertices.size()-f.first,"shape sparse range differs");std::set<unsigned> used;for(size_t i=f.first;i<size_t(f.first)+f.count;++i)Require(used.insert(m.shapeVertices[i].index).second,"duplicate vertex in shape frame");}
 count=s.Count(16);m.shapes.resize(count);Require(count==m.shapeNames.size(),"shape channel count differs");std::vector<unsigned char> covered(m.frames.size());
 for(size_t i=0;i<count;++i){auto& c=m.shapes[i];c.name=s.S();(void)s.N();c.first=unsigned(s.N());c.count=unsigned(s.N());Require(c.name==m.shapeNames[i]&&c.count&&c.first<=m.frames.size()&&c.count<=m.frames.size()-c.first,"shape channel frame range/name differs");for(size_t j=c.first;j<size_t(c.first)+c.count;++j)Require(!covered[j]++,"shape frame belongs to multiple channels");}
 Require(std::all_of(covered.begin(),covered.end(),[](auto n){return n==1;}),"orphan author shape frame");count=s.Count(4);Require(count==m.frames.size(),"shape frame weight count differs");for(auto& f:m.frames)f.weight=s.F();s.End();
 for(const auto& c:m.shapes){float last=-FLT_MAX;for(size_t i=c.first;i<size_t(c.first)+c.count;++i){Require(m.frames[i].weight>last,"shape frame weights do not increase");last=m.frames[i].weight;}}
 return m;
}
inline std::vector<std::array<float,16>> BindPoses(const Mesh& m){
 std::vector<std::array<float,16>> result(m.bones.size());Cursor c{m.bindPoseData};
 // Serialized Matrix4x4f fields are row-major; the public managed struct is
 // column-major. Copying these bytes without transposing breaks skinning.
 for(auto& matrix:result)for(size_t row=0;row<4;++row)for(size_t col=0;col<4;++col)matrix[col*4+row]=c.F();c.End();return result;
}
struct Texture {unsigned width=0,height=0,mips=0,format=4,filter=1,wrap=0,aniso=1;bool linear=false;float bias=0;Span pixels;};
inline Texture DecodeTexture(const Resource& r,const Settings& settings){
 Require(r.kind==4&&ResourceSize(r)>=80&&ResourceData(r)[6]=='2',"author payload is not an EFFRES2 Texture");Texture t;
 auto get=[&](const char* key,const char* fallback){auto i=settings.find(key);return i==settings.end()?std::string(fallback):i->second;};
 if(r.blocks.count("@texture")){auto c=ReadField(r,"@texture");auto version=c.N();Require(version==1||version==2,"unsupported author texture metadata");t.width=unsigned(c.N());t.height=unsigned(c.N());t.format=unsigned(c.N());t.mips=unsigned(c.N());if(version==2){Require(t.format==25||t.format==27,"unsupported compressed Texture2D format");(void)c.Take(32);}else Require(t.format==4,"unsupported legacy Texture2D format");c.End();}
 else{Require(get("format","")=="RGBA32","legacy Texture pixel encoding absent");t.width=Integer(get("width","0"));t.height=Integer(get("height","0"));t.mips=Integer(get("mip_count","0"));}
 t.filter=Integer(get("filter","1"));t.wrap=Integer(get("wrap","0"));t.aniso=Integer(get("aniso","1"));auto linear=get("linear","false");Require(linear=="true"||linear=="false","invalid texture color space");t.linear=linear=="true";t.bias=float(Number(get("mip_bias","0")));
 Require(t.width&&t.height&&t.width<=INT_MAX&&t.height<=INT_MAX&&t.mips&&(t.format==4||t.format==25||t.format==27)&&t.filter<=2&&t.wrap<=3&&t.aniso<=16&&std::isfinite(t.bias),"unsupported author texture layout/settings");
 unsigned maxMips=1;for(unsigned n=std::max(t.width,t.height);n>1;n>>=1)++maxMips;Require(t.mips<=maxMips,"too many author texture mip levels");
 size_t bytes=0;unsigned w=t.width,h=t.height;for(unsigned level=0;level<t.mips;++level){auto mipBytes=t.format==4?MultiplySize(MultiplySize(w,h),4):MultiplySize(MultiplySize(std::max(1u,(w+3)/4),std::max(1u,(h+3)/4)),16);bytes=AddSize(bytes,mipBytes);w=std::max(1u,w/2);h=std::max(1u,h/2);}t.pixels=Field(r,"pixels");Require(t.pixels.size==bytes&&bytes<=INT_MAX,"author texture pixels/mips differ");return t;
}
} // namespace EiemRuntimePayload
