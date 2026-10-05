#include "../../src/eiem_renderer_input_builder.h"
#include <fstream>
#include <iostream>
using namespace EiemNativeInput;
using namespace EiemDirectory;

int main(int argc, char** argv) {
  try {
    Require(argc == 5, "usage: shaped_frame_writer_regression <mod> <original.vfs> <frames.bin> <output.vfs>");
    const auto package = Load(std::filesystem::u8path(argv[1]));
    const auto source = Read(std::filesystem::u8path(argv[2]));
    const auto frames = Read(std::filesystem::u8path(argv[3]));
    const auto outputPath = std::filesystem::u8path(argv[4]);
    Require(!std::filesystem::exists(outputPath), "regression output already exists");
    const auto output = BuildMeshInput(source, package);
    const auto bundle = EndfieldInput::Decode(output.bytes);
    size_t tested = 0;
    double error = 0;
    for (const auto& part : output.parts) {
      const auto& mesh = package.meshes.at(package.parts.at(part.part).mesh);
      if (mesh.shapes.empty()) continue;
      auto member = std::find_if(bundle.members.begin(), bundle.members.end(), [&](const auto& m) { return m.name == part.cab; });
      Require(member != bundle.members.end(), "shaped Mesh member missing");
      const auto layout = ParseSerialized(member->bytes);
      const auto& type = layout.types.at(layout.objects.at(part.id).type);
      const auto value = ObjectValue(member->bytes, layout, part.id);
      const auto& resource = *package.resources.at(mesh.resource);
      const auto& vertex = Child(type, value, "m_VertexData");
      auto channelsValue = Child(type, vertex, "m_Channels");
      const auto& channels = ArrayValue(type, channelsValue).children;
      Require(Integer(Child(type, channels.at(1), "dimension")) == 3 &&
                  Integer(Child(type, channels.at(2), "dimension")) == 4 &&
                  Integer(Child(type, channels.at(2), "offset")) == 24,
              "shaped Mesh must use Float32 normal3/tangent4 before native deserialization");
      const auto& bytes = Child(type, vertex, "m_DataSize").raw;
      Require(frames.size() == size_t(mesh.vertices) * 32 && bytes.size() >= size_t(mesh.vertices) * 40,
              "real shaped frame extent");
      for (size_t i = 0; i < mesh.vertices; ++i)
        for (size_t c = 0; c < 7; ++c) {
          float expected{}, actual{};
          std::memcpy(&expected, frames.data() + i * 32 + 4 + c * 4, 4);
          std::memcpy(&actual, bytes.data() + i * 40 + 12 + c * 4, 4);
          Require(std::isfinite(actual), "nonfinite native frame expansion");
          error = std::max(error, std::abs(double(actual) - expected));
        }
      Require(error < 1e-7, "C++ expansion differs from independent packed-frame decoder");
      auto unchanged = value;
      Require(!ExpandShapedMeshFrames(type, unchanged) && EncodeValue(type, unchanged) == EncodeValue(type, value),
              "expanded shaped frames must be idempotent");
      auto packed = value;
      Child(type, packed, "m_VertexData") = AuthorField(type, value, resource, "m_VertexData");
      auto unshaped = packed;
      auto& shapes = Child(type, unshaped, "m_Shapes");
      auto& shapeChannels = ArrayValue(type, Child(type, shapes, "channels"));
      shapeChannels.children.clear(); shapeChannels.count = 0;
      const auto unshapedBytes = EncodeValue(type, unshaped);
      Require(!ExpandShapedMeshFrames(type, unshaped) && EncodeValue(type, unshaped) == unshapedBytes,
              "unshaped packed Mesh must be byte-identical");
      auto rejects = [&](Value bad) {
        try { ExpandShapedMeshFrames(type, bad); } catch (const std::exception&) { return; }
        throw std::runtime_error("malformed shaped vertex storage was accepted");
      };
      auto malformed = packed;
      Child(type, Child(type, malformed, "m_VertexData"), "m_DataSize").raw.pop_back();
      rejects(std::move(malformed));
      malformed = packed;
      auto& malformedData = Child(type, Child(type, malformed, "m_VertexData"), "m_DataSize").raw;
      WriteInteger(malformedData, 12, Le(malformedData, 12, 4) & ~uint64_t(0x40000000), 4);
      rejects(std::move(malformed));
      Resource expandedResource = resource;
      const auto encodedVertex = EncodeValue(type, vertex);
      auto& block = expandedResource.blocks.at("m_VertexData");
      block.offset = expandedResource.bytes.size(); block.size = encodedVertex.size();
      expandedResource.bytes.insert(expandedResource.bytes.end(), encodedVertex.begin(), encodedVertex.end());
      Require(PartBounds(type, value, resource, mesh, 0) == PartBounds(type, value, expandedResource, mesh, 0),
              "expanded author frame storage changed Renderer bounds");
      ++tested;
    }
    Require(tested, "real fixture must exercise a shaped author Mesh");
    std::ofstream file(outputPath, std::ios::binary);
    file.write((const char*)output.bytes.data(), output.bytes.size());
    Require(bool(file), "regression output write failed");
    std::cout << "PASS real shaped Mesh writer: meshes=" << tested << " maxFrameError=" << error
              << "; idempotence; unshaped no-op; extent/marker rejection; Renderer bounds preservation\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
