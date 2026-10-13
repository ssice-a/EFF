#pragma once
// Included inside EiemDirectory after Resource and its verified block helpers.
struct ShapeAnimation {
 std::string id,label,speedVariable;EiemKeyChord key{};double duration=0,speed=1;bool loop=true,autoplay=false;
 std::map<std::string,std::vector<float>> tracks;
};
inline std::pair<std::string,ShapeAnimation> DecodeShapeAnimation(const Resource& resource){
 Require(resource.kind==8&&resource.blocks.size()==1&&resource.blocks.count("@shape-animation"),"shape animation resource layout");
 const auto& block=resource.blocks.at("@shape-animation");size_t p=block.offset,end=p+block.size;
 auto number=[&](size_t n){Require(p<=end&&n<=end-p,"shape animation truncated");auto value=Le(resource.bytes,p,n);p+=n;return value;};
 auto strings=[&](){auto count=number(4);Require(count<=(end-p)/2,"shape animation string count");std::vector<std::string> result;for(uint64_t i=0;i<count;++i){auto n=number(2);Require(n<=end-p,"shape animation string range");result.push_back(Text(resource.bytes.data()+p,size_t(n)));p+=size_t(n);}return result;};
 auto names=strings();Require(names.size()==5&&Identifier(names[0])&&Identifier(names[1])&&EiemVariableName(names[4]),"shape animation names");ShapeAnimation result;result.id=names[1];result.label=names[2];result.speedVariable=names[4];
 if(!names[3].empty())Require(EiemParseKeyChord(names[3],&result.key),"shape animation shortcut invalid");
 auto scalar=[&](){auto bits=number(8);double value;std::memcpy(&value,&bits,8);Require(std::isfinite(value),"shape animation nonfinite timing");return value;};
 result.duration=scalar();result.speed=scalar();auto loop=number(1),autoPlay=number(1);Require(result.duration>0&&result.speed>0&&loop<=1&&autoPlay<=1,"shape animation playback domain");result.loop=loop!=0;result.autoplay=autoPlay!=0;
 auto count=number(4);Require(count&&count<=(end-p)/14,"shape animation track count");
 for(uint64_t i=0;i<count;++i){auto variable=strings();Require(variable.size()==1&&EiemVariableName(variable[0]),"shape animation track variable");auto samples=number(4);Require(samples>=2&&samples<=(end-p)/4,"shape animation sample range");std::vector<float> values;values.reserve(size_t(samples));for(uint64_t n=0;n<samples;++n){uint32_t bits=uint32_t(number(4));float value;std::memcpy(&value,&bits,4);Require(std::isfinite(value),"shape animation sample nonfinite");values.push_back(value);}Require(result.tracks.emplace(variable[0],std::move(values)).second,"shape animation duplicate track");}
 Require(p==end,"shape animation trailing bytes");return {names[0],std::move(result)};
}
