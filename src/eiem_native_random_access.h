#pragma once
#include "eiem_native_view_writer.h"
#include "eiem_serialized_writer.h"
#include "eiem_input_span_view.h"
#include <list>

// No game objects, author indexes or consumer lists. A source is only the
// file range/stream the game is actually asking to load. Untouched data blocks
// are neither read nor decoded. Limits bound a preparation task, not Mod size.
namespace EiemNativeRandomAccess {
using namespace EiemNativeInput;
using ReadRange = std::function<void(size_t, unsigned char*, size_t)>;
struct Limits {
  size_t directoryBytes = 16 * 1024 * 1024;
  size_t metadataBytes = 32 * 1024 * 1024;
  size_t blockBytes = 64 * 1024 * 1024;
  size_t fieldBytes = 16 * 1024 * 1024;
  size_t cacheBytes = 4 * 1024 * 1024;
  size_t readBytes = 64 * 1024 * 1024;
};
struct Statistics {
  size_t reads = 0, encodedBytes = 0, inflatedBytes = 0, inflatedBlocks = 0;
  size_t cacheHits = 0, cacheResident = 0, cachePeak = 0, largestBlock = 0;
  size_t pinnedBytes = 0;
};
class File {
  HANDLE handle_ = INVALID_HANDLE_VALUE;
  uint64_t offset_ = 0;
  size_t available_ = 0;
  BY_HANDLE_FILE_INFORMATION stamp_{};
  std::mutex mutex_;
public:
  File(const std::filesystem::path& path, uint64_t offset) : offset_(offset) {
    handle_ = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    try {
      Require(handle_ != INVALID_HANDLE_VALUE && GetFileInformationByHandle(handle_, &stamp_),
              "native range source open/stamp failed");
      uint64_t size = (uint64_t(stamp_.nFileSizeHigh) << 32) | stamp_.nFileSizeLow;
      Require(offset <= size && size - offset <= SIZE_MAX && size <= INT64_MAX,
              "native range source offset differs");
      available_ = size_t(size - offset);
    } catch (...) { if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_); throw; }
  }
  ~File() { if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_); }
  File(const File&) = delete;
  size_t Size() const { return available_; }
  void ReadAt(size_t at, unsigned char* output, size_t size) {
    Require(at <= available_ && size <= available_ - at, "native file read exceeds observed range");
    std::lock_guard<std::mutex> lock(mutex_);
    LARGE_INTEGER position{}; position.QuadPart = LONGLONG(offset_ + at);
    Require(SetFilePointerEx(handle_, position, nullptr, FILE_BEGIN), "native range seek failed");
    for (size_t p = 0; p < size;) {
      DWORD count = DWORD(std::min(size - p, size_t(1024 * 1024))), read = 0;
      Require(ReadFile(handle_, output + p, count, &read, nullptr) && read == count,
              "native range source truncated"); p += read;
    }
  }
  void VerifyStable() const {
    BY_HANDLE_FILE_INFORMATION now{};
    Require(GetFileInformationByHandle(handle_, &now) &&
        now.nFileSizeHigh == stamp_.nFileSizeHigh && now.nFileSizeLow == stamp_.nFileSizeLow &&
        now.ftLastWriteTime.dwHighDateTime == stamp_.ftLastWriteTime.dwHighDateTime &&
        now.ftLastWriteTime.dwLowDateTime == stamp_.ftLastWriteTime.dwLowDateTime,
        "native range source changed during inspection");
  }
};

class Reader {
  ReadRange source_;
  size_t sourceSize_ = 0;
  struct Cached { size_t index; std::shared_ptr<const Bytes> bytes; };
  std::list<Cached> cache_;
  size_t pinnedIndex_ = SIZE_MAX;
  std::shared_ptr<const Bytes> pinned_;
  void Source(size_t at, unsigned char* output, size_t size) {
    Require(at <= sourceSize_ && size <= sourceSize_ - at, "native range read exceeds source");
    Require(stats.encodedBytes <= limits.readBytes && size <= limits.readBytes - stats.encodedBytes,
            "native inspection read budget exhausted");
    source_(at, output, size); ++stats.reads; stats.encodedBytes += size;
  }
  std::shared_ptr<const Bytes> Block(size_t index) {
    if (pinned_ && pinnedIndex_ == index) { ++stats.cacheHits; return pinned_; }
    for (auto it = cache_.begin(); it != cache_.end(); ++it) if (it->index == index) {
      auto bytes = it->bytes; cache_.splice(cache_.begin(), cache_, it); ++stats.cacheHits; return bytes;
    }
    const auto& block = directory.blocks.at(index);
    Require(block.block.encoded <= limits.blockBytes && block.block.decoded <= limits.blockBytes,
            "native inspection block budget exhausted");
    pinned_.reset(); pinnedIndex_=SIZE_MAX; stats.pinnedBytes=0;
    Bytes encoded(block.block.encoded); Source(block.offset, encoded.data(), encoded.size());
    EndfieldInput::CryptBlock(encoded);
    auto decoded = std::make_shared<const Bytes>(Lz4(encoded.data(), encoded.size(), block.block.decoded, true));
    ++stats.inflatedBlocks; stats.inflatedBytes += decoded->size();
    stats.largestBlock = std::max(stats.largestBlock, decoded->size());
    if (decoded->size() <= limits.cacheBytes) {
      while (!cache_.empty() && decoded->size() > limits.cacheBytes - stats.cacheResident) {
        stats.cacheResident -= cache_.back().bytes->size(); cache_.pop_back();
      }
      cache_.push_front({index, decoded}); stats.cacheResident += decoded->size();
      stats.cachePeak = std::max(stats.cachePeak, stats.cacheResident);
    } else { pinned_=decoded; pinnedIndex_=index; stats.pinnedBytes=decoded->size(); }
    return decoded;
  }
public:
  Limits limits;
  Statistics stats;
  Bytes headerBytes;
  EndfieldInput::Header header;
  EiemNativeViewWriter::Directory directory;
  Reader(size_t sourceSize, ReadRange source, Limits settings = {})
      : source_(std::move(source)), sourceSize_(sourceSize), limits(settings) {
    Require(bool(source_) && sourceSize >= 48, "native range source/header absent");
    headerBytes.resize(48); Source(0, headerBytes.data(), 48);
    header = EndfieldInput::ReadHeader(headerBytes);
    Require(header.bytes <= sourceSize_, "native header exceeds source range");
    Require(header.compressedInfo <= limits.directoryBytes && header.decodedInfo <= limits.directoryBytes,
            "native inspection directory budget exhausted");
    auto at = header.flags & 0x80 ? header.bytes - header.compressedInfo : header.prefix;
    Bytes info(header.compressedInfo); Source(at, info.data(), info.size());
    directory = EiemNativeViewWriter::ReadDirectory(header, std::move(info));
  }
  void ReadAt(size_t at, unsigned char* output, size_t size) {
    Require(at <= directory.decoded && size <= directory.decoded - at, "native decoded read exceeds input");
    if (!size) return;
    Require(output, "native decoded read destination absent");
    auto found = std::upper_bound(directory.blocks.begin(), directory.blocks.end(), at,
        [](size_t position, const auto& block) { return position < block.decodedOffset; });
    Require(found != directory.blocks.begin(), "native decoded block absent"); --found;
    while (size) {
      const auto& block = *found;
      auto local = at - block.decodedOffset, count = std::min(size, block.block.decoded - local);
      Require(count, "native decoded block has no progress");
      if (!block.block.flags) Source(block.offset + local, output, count);
      else { auto bytes = Block(size_t(found - directory.blocks.begin())); std::memcpy(output, bytes->data() + local, count); }
      at += count; output += count; size -= count; ++found;
    }
  }
  Bytes ReadAt(size_t at, size_t size, size_t budget) {
    Require(size <= budget, "native inspection allocation budget exhausted");
    Bytes bytes(size); ReadAt(at, bytes.data(), size); return bytes;
  }
  void DiscardDecodedCache() {
    cache_.clear();pinned_.reset();pinnedIndex_=SIZE_MAX;stats.cacheResident=0;stats.pinnedBytes=0;
  }
  SerializedLayout Layout(const EiemNativeViewWriter::Member& member) {
    Require(member.size >= 48, "native member has no serialized header");
    auto prefix = ReadAt(member.offset, 48, 48);
    EiemNativeInput::Reader h(prefix, 20, true);
    auto metadata = size_t(h.Get(4)), full = h.Get(8), data = h.Get(8);
    Require(full == member.size && data <= full && data >= 48 && metadata <= data - 48,
            "native member serialized header differs");
    Require(metadata <= limits.metadataBytes, "native inspection metadata budget exhausted");
    prefix.resize(AddSize(48, metadata));
    ReadAt(member.offset + 48, prefix.data() + 48, metadata);
    return ParseSerialized(prefix, member.size);
  }
  struct Cursor {
    Reader& owner; size_t position, end, base;
    void Skip(size_t size) { Require(position <= end && size <= end - position, "native projected field exceeds object"); position += size; }
    uint32_t Count() { uint32_t value=0; Require(position <= end && 4 <= end-position,"native projected count truncated"); owner.ReadAt(position,reinterpret_cast<unsigned char*>(&value),4); position += 4; Require(value<=INT_MAX,"native projected count invalid"); return value; }
  };
  template<class InputCursor>
  static void Skip(const Type& type, size_t index, InputCursor& c, size_t depth = 0) {
    Require(index < type.nodes.size() && depth < type.nodes.size(), "native projected schema cycle");
    const auto& node = type.nodes[index];
    if (node.fixedWidth) { c.Skip(node.fixedWidth); return; }
    if (auto width = Primitive(node.type)) c.Skip(width);
    else if (node.type == "string" || node.type == "TypelessData") { auto n = c.Count(); c.Skip(n); }
    else if (node.type == "Array") {
      Require(node.children.size() == 2, "native projected array schema differs");
      auto n = c.Count(); auto element = node.children[1]; const auto& child = type.nodes[element];
      auto elementWidth = child.fixedWidth ? child.fixedWidth : Primitive(child.type);
      if (elementWidth && !child.align) { Require(n <= (c.end-c.position)/elementWidth,"native projected array count exceeds field"); c.Skip(size_t(n)*elementWidth); }
      else for (uint32_t i = 0; i < n; ++i) Skip(type, element, c, depth + 1);
    } else {
      Require(!node.children.empty(), "native projected unknown leaf: " + node.type);
      for (auto child : node.children) Skip(type, child, c, depth + 1);
    }
    if (node.align || node.type == "string") c.Skip((size_t(0) - (c.position-c.base)) & 3);
  }
  std::map<std::string,Field> Ranges(const EiemNativeViewWriter::Member& member,const SerializedLayout& layout,int64_t id) {
    const auto& object=layout.objects.at(id);const auto& type=layout.types.at(object.type);auto begin=AddSize(member.offset,object.offset);
    Cursor cursor{*this,begin,AddSize(begin,object.size),begin};std::map<std::string,Field> fields;
    for(auto child:type.nodes.at(0).children){auto at=cursor.position;Skip(type,child,cursor);
      Require(fields.emplace(type.nodes[child].name,Field{child,at,cursor.position-at}).second,"native object top-level field duplicated");
    }
    if(type.nodes.at(0).align)cursor.Skip((size_t(0)-(cursor.position-begin))&3);
    Require(cursor.position==cursor.end,"native object field ranges do not consume payload");return fields;
  }
  // Author outputs use native uncompressed blocks. Expose their decoded bytes
  // as file spans; neither opening the view nor a consumer graft reads pixels,
  // vertices or shape deltas. The callback must retain its immutable file lease.
  std::shared_ptr<const EiemInputSpanView::View> UncompressedView(ReadRange source) const {
    std::vector<EiemInputSpanView::Span> spans;
    for(const auto& block:directory.blocks){Require(!block.block.flags&&block.block.decoded==block.block.encoded,"streamable author input contains compressed blocks");
      if(block.block.decoded)spans.push_back({0,block.offset,block.block.decoded,{}});
    }
    auto size=header.bytes;return std::make_shared<const EiemInputSpanView::View>(std::move(spans),std::vector<size_t>{size},
      [source=std::move(source)](size_t id,size_t at,unsigned char* out,size_t n){Require(!id,"author native view source id differs");source(at,out,n);});
  }
  std::map<std::string, Value> Fields(const EiemNativeViewWriter::Member& member,
      const SerializedLayout& layout, int64_t id, const std::set<std::string>& wanted) {
    const auto& object = layout.objects.at(id); const auto& type = layout.types.at(object.type);
    auto begin = AddSize(member.offset, object.offset);
    Cursor cursor{*this, begin, AddSize(begin, object.size), begin}; std::map<std::string, Value> result;
    for (auto child : type.nodes.at(0).children) {
      auto at = cursor.position; Skip(type, child, cursor);
      const auto& name = type.nodes[child].name; if (!wanted.count(name)) continue;
      auto size = cursor.position-at, padding = (at-begin)&3;
      Require(size <= limits.fieldBytes, "native inspection field budget exhausted");
      Bytes bytes(AddSize(padding,size)); ReadAt(at,bytes.data()+padding,size);
      EiemNativeInput::Reader value(bytes,padding); auto decoded=ReadValue(type,child,value,0);
      Require(value.p==bytes.size()&&result.emplace(name,std::move(decoded)).second,"native projected field extent/identity differs");
      if (result.size() == wanted.size()) break;
    }
    Require(result.size()==wanted.size(),"native projected field absent"); return result;
  }
  Value Object(const EiemNativeViewWriter::Member& member,const SerializedLayout& layout,int64_t id) {
    const auto& object=layout.objects.at(id);const auto& type=layout.types.at(object.type);
    return DecodeValue(type,0,ReadAt(AddSize(member.offset,object.offset),object.size,limits.fieldBytes));
  }
  // Match ObjectFields' optional projection semantics: different native object
  // classes need not contain every requested field. Required fields are still
  // checked by the consumer's Child() access, before any input is published.
  Value Project(const EiemNativeViewWriter::Member& member,const SerializedLayout& layout,int64_t id,
      const std::set<std::string>& wanted) {
    const auto& type=layout.types.at(layout.objects.at(id).type);std::set<std::string> present;
    for(auto child:type.nodes.at(0).children)if(wanted.count(type.nodes.at(child).name))present.insert(type.nodes.at(child).name);
    Value result;auto values=present.empty()?std::map<std::string,Value>{}:Fields(member,layout,id,present);
    for(auto child:type.nodes.at(0).children){auto found=values.find(type.nodes.at(child).name);if(found!=values.end())result.children.push_back(std::move(found->second));}
    return result;
  }
  std::vector<Asset> Assets(const EiemNativeViewWriter::Member& member, const SerializedLayout& layout) {
    std::vector<Asset> result;
    for (auto id : layout.order) {
      const auto& type = layout.types.at(layout.objects.at(id).type); if (type.classId != 142) continue;
      auto fields = Fields(member,layout,id,{"m_Container"});
      for (const auto& row : ArrayValue(type,fields.at("m_Container")).children) {
        const auto& key=Child(type,row,"first"); const auto& info=Child(type,row,"second");
        const auto& ptr=Child(type,info,"asset"); auto file=Integer(Child(type,ptr,"m_FileID"));
        Require(file<=layout.externals.size(),"native container external index differs");
        Asset asset; asset.path=Text(key.raw.data(),key.raw.size());
        std::transform(asset.path.begin(),asset.path.end(),asset.path.begin(),[](unsigned char c){return char(std::tolower(c));});
        asset.cab=file?layout.externals[size_t(file)-1]:member.name;
        asset.pathId=int64_t(Integer(Child(type,ptr,"m_PathID")));
        auto target=layout.objects.find(asset.pathId);
        if (!file && target!=layout.objects.end()) { const auto& t=layout.types.at(target->second.type); asset.classId=t.classId; asset.type=t.hash;
          if (std::any_of(t.nodes[0].children.begin(),t.nodes[0].children.end(),[&](size_t n){return t.nodes[n].name=="m_Name";})) {
            auto name=Fields(member,layout,asset.pathId,{"m_Name"}); const auto& v=name.at("m_Name"); asset.asset=Text(v.raw.data(),v.raw.size());
          }
        }
        result.push_back(std::move(asset));
      }
    }
    return result;
  }
  std::map<uint64_t,Asset> HashAssets(const EiemNativeViewWriter::Member& member, const SerializedLayout& layout) {
    std::map<uint64_t,Asset> result;
    for (auto id : layout.order) {
      const auto& type=layout.types.at(layout.objects.at(id).type); if(type.classId!=142)continue;
      if(!std::any_of(type.nodes[0].children.begin(),type.nodes[0].children.end(),[&](size_t n){return type.nodes[n].name=="m_HashContainer";}))continue;
      auto fields=Fields(member,layout,id,{"m_HashContainer"});
      for(const auto& row:ArrayValue(type,fields.at("m_HashContainer")).children){
        auto key=Integer(Child(type,row,"first"));const auto& ptr=Child(type,Child(type,row,"second"),"asset");
        auto file=Integer(Child(type,ptr,"m_FileID"));Require(file<=layout.externals.size(),"native hash container external index differs");
        Asset asset;asset.cab=file?layout.externals[size_t(file)-1]:member.name;asset.pathId=int64_t(Integer(Child(type,ptr,"m_PathID")));
        auto target=layout.objects.find(asset.pathId);
        if(!file&&target!=layout.objects.end()){const auto& t=layout.types.at(target->second.type);asset.classId=t.classId;asset.type=t.hash;}
        auto inserted=result.emplace(key,asset);Require(inserted.second||(inserted.first->second.cab==asset.cab&&inserted.first->second.pathId==asset.pathId),"native hash container ambiguous key");
      }
    }
    return result;
  }
};
} // namespace EiemNativeRandomAccess
