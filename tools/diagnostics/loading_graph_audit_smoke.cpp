#include <iostream>
#include <functional>
#include "eiem_loading_graph_audit.h"
using namespace EiemLoadingGraphAudit;
static void Rejects(const std::function<void()>& call, const char* reason) {
  bool rejected = false; try { call(); } catch (const std::exception&) { rejected = true; }
  Require(rejected, reason);
}
int main() {
  try {
    Type type;
    type.classId = 137;
    type.nodes = {{"Renderer", "Base", false, {1,4}},
                  {"PPtr<Mesh>", "m_Mesh", false, {2,3}},
                  {"SInt32", "m_FileID", false, {}},
                  {"SInt64", "m_PathID", false, {}},
                  {"StreamingInfo", "m_StreamData", false, {5,6,7}},
                  {"UInt64", "offset", false, {}}, {"UInt32", "size", false, {}},
                  {"string", "path", true, {}}};
    Bytes bytes;
    AppendInteger(bytes, 2, 4); AppendInteger(bytes, uint64_t(int64_t(-17)), 8);
    AppendInteger(bytes, 16, 8); AppendInteger(bytes, 32, 4);
    std::string path = "archive:/CAB-unit/CAB-unit.resS";
    AppendInteger(bytes, path.size(), 4); bytes.insert(bytes.end(), path.begin(), path.end());
    bytes.resize((bytes.size() + 3) & ~size_t(3));
    SerializedLayout layout; layout.types = {type}; layout.objects.emplace(-101, Object{-101,0,bytes.size(),0});
    auto refs = ReadReferences(bytes, layout);
    Require(refs.unparsed.empty() && refs.pointers.size() == 1 && refs.streams.size() == 1,
            "reference and stream traversal failed");
    Require(refs.pointers[0].fileId == 2 && refs.pointers[0].pathId == -17 && refs.pointers[0].objectId == -101,
            "signed reference identity changed");
    Require(refs.streams[0].path == path && refs.streams[0].offset == 16 && refs.streams[0].size == 32,
            "stream range changed");
    auto corrupt = bytes; corrupt.pop_back();
    layout.objects.at(-101).size = corrupt.size();
    auto rejected = ReadReferences(corrupt, layout);
    Require(rejected.unparsed.size() == 1 && rejected.pointers.empty() && rejected.streams.empty(),
            "partial object reference evidence escaped rejection");
    Type unknown; unknown.classId = 114;
    unknown.nodes = {{"MonoBehaviour", "Base", false, {1}}, {"ReferencedObjectData", "data", false, {}}};
    layout.types = {unknown}; layout.objects.at(-101).size = bytes.size();
    auto opaque = ReadReferences(bytes, layout);
    Require(opaque.unparsed.size() == 1 && opaque.pointers.empty(), "opaque managed object silently accepted");
    Type scalar; scalar.classId = 1;
    scalar.nodes = {{"GameObject", "Base", false, {1}}, {"UInt32", "tag", false, {}}};
    layout.types = {scalar}; layout.objects.at(-101).size = 4;
    auto irrelevant = ReadReferences(Bytes{0,0,0,0}, layout);
    Require(irrelevant.unparsed.empty() && irrelevant.pointers.empty(), "unreferenced primitive traversal failed");
    Bytes external(48);
    AppendInteger(external, 0, 4); AppendInteger(external, 1, 4); external.push_back(0);
    size_t guidOffset = external.size(); external.resize(external.size() + 16); external[guidOffset + 8] = 14;
    AppendInteger(external, 0, 4);
    std::string externalPath = "archive:/CAB-source/CAB-source";
    external.insert(external.end(), externalPath.begin(), externalPath.end()); external.push_back(0);
    layout = {}; layout.tableEnd = 48; layout.metadataSize = external.size() - 48;
    layout.externals = {"CAB-source"};
    auto entries = ReadExternals(external, layout);
    Require(entries.size() == 1 && entries[0].fileId == 1 && entries[0].path == externalPath &&
                entries[0].guid == "00000000000000000E00000000000000" && entries[0].guidOffset == guidOffset,
            "external GUID/path offsets changed");
    layout.externals = {"wrong"}; Rejects([&] { ReadExternals(external, layout); }, "external basename mismatch accepted");
    layout.externals = {"CAB-source"}; external.pop_back(); --layout.metadataSize;
    Rejects([&] { ReadExternals(external, layout); }, "unterminated external string accepted");
    std::cout << "LOADING-GRAPH-AUDIT-SMOKE signedPathIds=1 streamRange=1 partialEvidenceRejected=1 opaqueObjectRejected=1 externalGuidPreserved=1 truncationRejected=1\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
