#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct EiemModelBlob {
  std::vector<uint8_t> bytes;

  size_t Align(size_t alignment = 4) {
    while (bytes.size() % alignment) bytes.push_back(0);
    return bytes.size();
  }
  size_t Add(const void *data, size_t size, size_t alignment = 4) {
    Align(alignment);
    const size_t offset = bytes.size();
    if (data && size) {
      const uint8_t *src = static_cast<const uint8_t *>(data);
      bytes.insert(bytes.end(), src, src + size);
    } else {
      bytes.resize(bytes.size() + size, 0);
    }
    return offset;
  }
};

struct EiemGltfView {
  size_t offset;
  size_t length;
  int target;
};
struct EiemGltfAccessor {
  int view;
  size_t offset;
  size_t count;
  int componentType;
  const char *type;
  bool normalized;
};
struct EiemTextureExport {
  std::string property;
  std::string name;
  int width = 0;
  int height = 0;
};
struct EiemMaterialExport {
  std::string name;
  std::string shader;
  std::vector<std::pair<std::string, std::string>> textures;
  std::vector<EiemTextureExport> textureDetails;
  std::vector<std::pair<std::string, std::string>> properties;
};
struct EiemExportMesh {
  std::string name;
  std::string hierarchy;
  void *mesh;
  void *renderer;
  std::vector<Vector3> positions;
  std::vector<Vector3> normals;
  std::vector<Vector4> tangents;
  std::vector<Vector2> uv;
  std::vector<float> colors;
  std::vector<uint32_t> indices;
  std::vector<std::vector<uint32_t>> submeshIndices;
  std::vector<BoneWeight> boneWeights;
  std::vector<Matrix4x4> bindposes;
  std::vector<std::string> bones;
  std::vector<std::string> materials;
  std::vector<std::string> materialShaders;
  std::vector<std::string> materialTextureProperties;
  std::vector<std::string> materialTextures;
  std::vector<std::string> materialParameters;
  std::vector<EiemMaterialExport> materialDetails;
  std::vector<std::string> blendShapes;
  std::vector<float> blendShapeWeights;
  std::string rootBone;
  std::string rendererPath;
  std::string identity;
  int lod = -1;
};

static int32_t EiemUnboxInt(void *boxed) {
  __try {
    return boxed ? *(int32_t *)((char *)boxed + 16) : 0;
  } __except (1) {
    return 0;
  }
}

static float EiemUnboxFloat(void *boxed) {
  __try {
    return boxed ? *(float *)((char *)boxed + 16) : 0.0f;
  } __except (1) {
    return 0.0f;
  }
}

static bool EiemReadBoxedColor(void *boxed, Color &out) {
  __try {
    if (!boxed) return false;
    out = *(Color *)((char *)boxed + 16);
    return true;
  } __except (1) {
    return false;
  }
}

static bool EiemReadBoxedVector(void *boxed, Vector4 &out) {
  __try {
    if (!boxed) return false;
    out = *(Vector4 *)((char *)boxed + 16);
    return true;
  } __except (1) {
    return false;
  }
}

static bool EiemReadMeshDataFallback(void *mesh, EiemExportMesh &out) {
  if (!mesh || !g_mesh_acquireReadOnlyMeshData ||
      !g_meshDataArray_get_Item || !g_meshDataArray_Dispose ||
      !g_meshData_get_vertexCount || !g_meshData_get_subMeshCount ||
      !g_meshData_GetIndexCount || !g_meshData_CopyAttributeIntoPtr ||
      !g_meshData_CopyIndicesIntoPtr)
    return false;

  void *meshParams[] = {&mesh};
  void *arrayBoxed = Invoke(g_mesh_acquireReadOnlyMeshData, nullptr, meshParams);
  if (!arrayBoxed) return false;

  bool success = false;
  {
    void **ptrs = *(void ***)((char *)arrayBoxed + 16);
    int32_t length = *(int32_t *)((char *)arrayBoxed + 24);
    if (!ptrs || length <= 0 || length > 8 || !ptrs[0]) {
      Invoke(g_meshDataArray_Dispose, arrayBoxed);
      return false;
    }
    int32_t dataIndex = 0;
    void *itemParams[] = {&dataIndex};
    void *dataBoxed = Invoke(g_meshDataArray_get_Item, arrayBoxed, itemParams);
    if (!dataBoxed) {
      Invoke(g_meshDataArray_Dispose, arrayBoxed);
      return false;
    }
    void *dataPtr = *(void **)((char *)dataBoxed + 16);
    int32_t vertexCount = EiemUnboxInt(Invoke(g_meshData_get_vertexCount, dataBoxed));
    int32_t subMeshCount = EiemUnboxInt(Invoke(g_meshData_get_subMeshCount, dataBoxed));
    if (!dataPtr || vertexCount <= 0 || vertexCount > 2000000 ||
        subMeshCount <= 0 || subMeshCount > 64) {
      Invoke(g_meshDataArray_Dispose, arrayBoxed);
      return false;
    }

    auto copyAttribute = [&](int attribute, int dimension, void *destination) {
      int format = 0; // VertexAttributeFormat.Float32
      void *params[] = {&dataPtr, &attribute, &format, &dimension, &destination};
      return Invoke(g_meshData_CopyAttributeIntoPtr, nullptr, params) != nullptr ||
             destination != nullptr;
    };
    out.positions.resize((size_t)vertexCount);
    copyAttribute(0, 3, out.positions.data());
    if (out.positions.empty()) {
      Invoke(g_meshDataArray_Dispose, arrayBoxed);
      return false;
    }
    out.normals.resize((size_t)vertexCount);
    if (!copyAttribute(1, 3, out.normals.data())) out.normals.clear();
    out.tangents.resize((size_t)vertexCount);
    if (!copyAttribute(2, 4, out.tangents.data())) out.tangents.clear();
    out.uv.resize((size_t)vertexCount);
    if (!copyAttribute(4, 2, out.uv.data())) out.uv.clear();
    std::vector<Color> colors((size_t)vertexCount);
    if (copyAttribute(3, 4, colors.data())) {
      out.colors.reserve(colors.size() * 4);
      for (const Color &color : colors) {
        out.colors.push_back(color.r);
        out.colors.push_back(color.g);
        out.colors.push_back(color.b);
        out.colors.push_back(color.a);
      }
    }

    out.submeshIndices.resize((size_t)subMeshCount);
    for (int32_t subMesh = 0; subMesh < subMeshCount; ++subMesh) {
      void *indexCountParams[] = {&dataPtr, &subMesh};
      int32_t indexCount = EiemUnboxInt(
          Invoke(g_meshData_GetIndexCount, nullptr, indexCountParams));
      if (indexCount <= 0 || indexCount > vertexCount * 3) continue;
      std::vector<int32_t> signedIndices((size_t)indexCount);
      bool applyBaseVertex = false;
      int32_t destinationStart = 0;
      void *destination = signedIndices.data();
      void *indexParams[] = {&dataPtr, &subMesh, &applyBaseVertex,
                             &destinationStart, &destination};
      Invoke(g_meshData_CopyIndicesIntoPtr, nullptr, indexParams);
      auto &indices = out.submeshIndices[(size_t)subMesh];
      indices.reserve(signedIndices.size());
      for (int32_t index : signedIndices)
        if (index >= 0) indices.push_back((uint32_t)index);
      out.indices.insert(out.indices.end(), indices.begin(), indices.end());
    }
    success = !out.positions.empty() && !out.indices.empty();
    Invoke(g_meshDataArray_Dispose, arrayBoxed);
  }
  return success;
}

// The IL2CPP array header used by Unity on this build is 24 bytes followed by
// the first element at +32. This helper copies only bounded POD arrays.
template <typename T>
static bool EiemCopyManagedArray(void *array, std::vector<T> &out,
                                 size_t maxCount = 2000000) {
  out.clear();
  if (!array) return false;
  __try {
    int32_t count = *(int32_t *)((char *)array + 24);
    if (count < 0 || (size_t)count > maxCount) return false;
    T *data = (T *)((char *)array + 32);
    out.assign(data, data + count);
    return true;
  } __except (1) {
    out.clear();
    return false;
  }
}

static bool EiemReadMeshExport(void *mesh, void *renderer,
                               const EiemMeshObservation &observation,
                               EiemExportMesh &out) {
  if (!mesh) return false;
  out = {};
  out.mesh = mesh;
  out.renderer = renderer;
  out.name = observation.meshName[0] ? observation.meshName : "Mesh";
  out.hierarchy = observation.hierarchyPath;
  out.rendererPath = observation.hierarchyPath;
  out.identity = out.hierarchy + "|" + out.name;
  const char *lodMarker = strstr(out.hierarchy.c_str(), "lod");
  if (lodMarker && lodMarker[3] >= '0' && lodMarker[3] <= '9')
    out.lod = atoi(lodMarker + 3);

  void *vertices = g_mesh_get_vertices ? Invoke(g_mesh_get_vertices, mesh) : nullptr;
  const bool managedReadable =
      EiemCopyManagedArray(vertices, out.positions) && !out.positions.empty();
  if (!managedReadable && !EiemReadMeshDataFallback(mesh, out)) return false;
  if (!managedReadable) {
    if (renderer && g_renderer_get_sharedMaterials) {
      void *materials = Invoke(g_renderer_get_sharedMaterials, renderer);
      std::vector<void *> materialObjects;
      if (EiemCopyManagedArray(materials, materialObjects, 64)) {
        for (void *material : materialObjects) {
          char name[192] = {};
          if (material && g_object_get_name)
            ReadStrUtf8(Invoke(g_object_get_name, material), name, sizeof(name));
          const char *materialName = name[0] ? name : "<unnamed material>";
          out.materials.emplace_back(materialName);
          EiemMaterialExport detail;
          detail.name = materialName;
          out.materialDetails.push_back(std::move(detail));
        }
      }
    }
    return !out.indices.empty();
  }
  if (g_mesh_get_normals)
    EiemCopyManagedArray(Invoke(g_mesh_get_normals, mesh), out.normals,
                         out.positions.size());
  if (g_mesh_get_tangents)
    EiemCopyManagedArray(Invoke(g_mesh_get_tangents, mesh), out.tangents,
                         out.positions.size());
  if (g_mesh_get_uv)
    EiemCopyManagedArray(Invoke(g_mesh_get_uv, mesh), out.uv,
                         out.positions.size());
  if (g_mesh_get_colors) {
    std::vector<Color> colors;
    if (EiemCopyManagedArray(Invoke(g_mesh_get_colors, mesh), colors,
                             out.positions.size())) {
      out.colors.reserve(colors.size() * 4);
      for (const Color &color : colors) {
        out.colors.push_back(color.r);
        out.colors.push_back(color.g);
        out.colors.push_back(color.b);
        out.colors.push_back(color.a);
      }
    }
  }
  if (g_mesh_get_triangles)
    EiemCopyManagedArray(Invoke(g_mesh_get_triangles, mesh), out.indices,
                         out.positions.size() * 12);
  if (g_mesh_GetTriangles && g_mesh_get_subMeshCount) {
    int32_t subMeshCount = EiemUnboxInt(Invoke(g_mesh_get_subMeshCount, mesh));
    if (subMeshCount > 0 && subMeshCount <= 64) {
      out.submeshIndices.resize((size_t)subMeshCount);
      for (int32_t subMesh = 0; subMesh < subMeshCount; ++subMesh) {
        void *params[] = {&subMesh};
        EiemCopyManagedArray(Invoke(g_mesh_GetTriangles, mesh, params),
                             out.submeshIndices[(size_t)subMesh],
                             out.positions.size() * 3);
      }
      bool anySubmesh = false;
      for (const auto &submesh : out.submeshIndices)
        if (!submesh.empty()) { anySubmesh = true; break; }
      if (!anySubmesh) out.submeshIndices.clear();
    }
  }
  if (g_mesh_get_boneWeights)
    EiemCopyManagedArray(Invoke(g_mesh_get_boneWeights, mesh),
                         out.boneWeights, out.positions.size());
  if (g_mesh_get_bindposes)
    EiemCopyManagedArray(Invoke(g_mesh_get_bindposes, mesh), out.bindposes, 4096);
  if (g_mesh_get_blendShapeCount && g_mesh_GetBlendShapeName) {
    const int32_t blendShapeCount =
        EiemUnboxInt(Invoke(g_mesh_get_blendShapeCount, mesh));
    if (blendShapeCount >= 0 && blendShapeCount <= 4096) {
      out.blendShapes.reserve((size_t)blendShapeCount);
      out.blendShapeWeights.reserve((size_t)blendShapeCount);
      for (int32_t index = 0; index < blendShapeCount; ++index) {
        void *params[] = {&index};
        char blendName[192] = {};
        void *name = Invoke(g_mesh_GetBlendShapeName, mesh, params);
        if (name) ReadStrUtf8(name, blendName, sizeof(blendName));
        out.blendShapes.emplace_back(blendName[0] ? blendName : "<unnamed blendshape>");
        float weight = 0.0f;
        if (renderer && g_smr_GetBlendShapeWeight) {
          void *boxed = Invoke(g_smr_GetBlendShapeWeight, renderer, params);
          weight = EiemUnboxFloat(boxed);
        }
        out.blendShapeWeights.push_back(weight);
      }
    }
  }

  if (renderer && strcmp(observation.rendererType, "SkinnedMeshRenderer") == 0 &&
      g_smr_get_bones) {
    std::vector<void *> boneObjects;
    if (EiemCopyManagedArray(Invoke(g_smr_get_bones, renderer), boneObjects, 512)) {
      for (void *bone : boneObjects) {
        char boneName[192] = {};
        if (bone && g_object_get_name)
          ReadStrUtf8(Invoke(g_object_get_name, bone), boneName, sizeof(boneName));
        out.bones.emplace_back(boneName[0] ? boneName : "<unnamed bone>");
      }
    }
    if (g_smr_get_rootBone) {
      void *rootBone = Invoke(g_smr_get_rootBone, renderer);
      char rootName[192] = {};
      if (rootBone && g_object_get_name)
        ReadStrUtf8(Invoke(g_object_get_name, rootBone), rootName, sizeof(rootName));
      if (rootName[0]) out.rootBone = rootName;
    }
  }

  if (g_renderer_get_sharedMaterials && renderer) {
    void *materials = Invoke(g_renderer_get_sharedMaterials, renderer);
    std::vector<void *> materialObjects;
    if (EiemCopyManagedArray(materials, materialObjects, 64)) {
      for (void *material : materialObjects) {
        char name[192] = {};
        if (material && g_object_get_name)
          ReadStrUtf8(Invoke(g_object_get_name, material), name, sizeof(name));
        const char *materialName = name[0] ? name : "<unnamed material>";
        out.materials.emplace_back(materialName);
        EiemMaterialExport detail;
        detail.name = materialName;
        char shaderName[256] = {};
        if (material && g_material_get_shader) {
          void *shader = Invoke(g_material_get_shader, material);
          if (shader && g_object_get_name)
            ReadStrUtf8(Invoke(g_object_get_name, shader), shaderName,
                        sizeof(shaderName));
          if (!shaderName[0] && shader)
            TraceDescribeObject(shader, shaderName, sizeof(shaderName));
        }
        detail.shader = shaderName[0] ? shaderName : "<unresolved>";
        out.materialShaders.emplace_back(detail.shader);

        std::string textureProperties;
        std::string textureValues;
        std::string parameters;
        if (material && g_material_GetTexturePropertyNames) {
          void *namesArray = Invoke(g_material_GetTexturePropertyNames, material);
          std::vector<void *> names;
          if (EiemCopyManagedArray(namesArray, names, 128)) {
            for (void *property : names) {
              char propertyName[192] = {};
              if (property) ReadStrUtf8(property, propertyName, sizeof(propertyName));
              if (!propertyName[0]) continue;
              std::string propertyValue;
              void *propertyString = il2cpp_string_new ? il2cpp_string_new(propertyName) : nullptr;
              if (!textureProperties.empty()) textureProperties += ";";
              textureProperties += propertyName;
              if (g_material_GetTexture) {
                void *textureParams[] = {propertyString};
                void *texture = propertyString ? Invoke(g_material_GetTexture, material, textureParams) : nullptr;
                char textureText[256] = {};
                if (texture) {
                  if (g_object_get_name)
                    ReadStrUtf8(Invoke(g_object_get_name, texture), textureText, sizeof(textureText));
                  if (!textureText[0]) TraceDescribeObject(texture, textureText, sizeof(textureText));
                }
                propertyValue = textureText[0] ? textureText : "<null>";
                if (!textureValues.empty()) textureValues += ";";
                textureValues += propertyName;
                textureValues += "=";
                textureValues += propertyValue;
                detail.textures.emplace_back(propertyName, propertyValue);
                EiemTextureExport textureDetail;
                textureDetail.property = propertyName;
                textureDetail.name = propertyValue;
                if (texture && g_texture_get_width)
                  textureDetail.width = EiemUnboxInt(Invoke(g_texture_get_width, texture));
                if (texture && g_texture_get_height)
                  textureDetail.height = EiemUnboxInt(Invoke(g_texture_get_height, texture));
                detail.textureDetails.push_back(std::move(textureDetail));
              }
              bool hasScalarValue = false;
              if (propertyString && g_material_GetColor) {
                void *params[] = {propertyString};
                Color value = {};
                if (EiemReadBoxedColor(Invoke(g_material_GetColor, material, params), value)) {
                  char text[160] = {};
                  snprintf(text, sizeof(text), "color(%.6g,%.6g,%.6g,%.6g)", value.r, value.g, value.b, value.a);
                  detail.properties.emplace_back(propertyName, text);
                  hasScalarValue = true;
                }
              }
              if (propertyString && !hasScalarValue && g_material_GetVector) {
                void *params[] = {propertyString};
                Vector4 value = {};
                if (EiemReadBoxedVector(Invoke(g_material_GetVector, material, params), value)) {
                  char text[160] = {};
                  snprintf(text, sizeof(text), "vector(%.6g,%.6g,%.6g,%.6g)", value.x, value.y, value.z, value.w);
                  detail.properties.emplace_back(propertyName, text);
                  hasScalarValue = true;
                }
              }
              if (propertyString && !hasScalarValue && g_material_GetFloat) {
                void *params[] = {propertyString};
                void *boxed = Invoke(g_material_GetFloat, material, params);
                if (boxed) {
                  char text[96] = {};
                  snprintf(text, sizeof(text), "float(%.6g)", EiemUnboxFloat(boxed));
                  detail.properties.emplace_back(propertyName, text);
                  hasScalarValue = true;
                }
              }
              if (propertyString && !hasScalarValue && g_material_GetInt) {
                void *params[] = {propertyString};
                void *boxed = Invoke(g_material_GetInt, material, params);
                if (boxed) {
                  char text[96] = {};
                  snprintf(text, sizeof(text), "int(%d)", EiemUnboxInt(boxed));
                  detail.properties.emplace_back(propertyName, text);
                  hasScalarValue = true;
                }
              }
              if (!hasScalarValue && propertyValue.empty())
                detail.properties.emplace_back(propertyName, "unresolved");
            }
          }
        }
        if (g_material_GetFloat || g_material_GetInt || g_material_GetColor || g_material_GetVector) {
          parameters = "readers=";
          parameters += g_material_GetColor ? "color," : "";
          parameters += g_material_GetVector ? "vector," : "";
          parameters += g_material_GetFloat ? "float," : "";
          parameters += g_material_GetInt ? "int" : "";
        }
        out.materialTextureProperties.emplace_back(textureProperties);
        out.materialTextures.emplace_back(textureValues);
        out.materialParameters.emplace_back(parameters);
        if (detail.shader.empty()) detail.shader = "<unresolved>";
        out.materialDetails.push_back(std::move(detail));
      }
    }
  }
  return !out.indices.empty();
}

static unsigned long long EiemIdentityHash(const std::string &value) {
  unsigned long long hash = 1469598103934665603ull;
  for (unsigned char ch : value) {
    hash ^= ch;
    hash *= 1099511628211ull;
  }
  return hash;
}

static int EiemAddGltfAccessor(std::vector<EiemGltfAccessor> &accessors,
                               std::vector<EiemGltfView> &views,
                               EiemModelBlob &blob, const void *data,
                               size_t elementSize, size_t count,
                               int componentType, const char *type,
                               int target, bool normalized = false) {
  const size_t offset = blob.Add(data, elementSize * count);
  const int view = (int)views.size();
  views.push_back({offset, elementSize * count, target});
  accessors.push_back({view, 0, count, componentType, type, normalized});
  return (int)accessors.size() - 1;
}

static void EiemWriteGltf(const char *directory,
                          const std::vector<EiemExportMesh> &meshes,
                          const std::vector<EiemMeshObservation> &observations) {
  if (!directory || meshes.empty()) return;
  EiemModelBlob blob;
  std::vector<EiemGltfView> views;
  std::vector<EiemGltfAccessor> accessors;
  struct Primitive { int position, normal, tangent, uv, color, joints, weights, indices, material; };
  struct MeshInfo { std::string name; std::vector<Primitive> primitives; };
  std::vector<MeshInfo> gltfMeshes;
  std::vector<std::string> gltfMaterials;

  for (const EiemExportMesh &mesh : meshes) {
    for (const std::string &material : mesh.materials) {
      bool known = false;
      for (const std::string &existing : gltfMaterials) {
        if (existing == material) {
          known = true;
          break;
        }
      }
      if (!known) gltfMaterials.push_back(material);
    }
  }

  for (const EiemExportMesh &mesh : meshes) {
    MeshInfo info;
    info.name = mesh.name;
    Primitive base = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    base.position = EiemAddGltfAccessor(accessors, views, blob,
                                        mesh.positions.data(), sizeof(Vector3),
                                        mesh.positions.size(), 5126, "VEC3", 34962);
    if (mesh.normals.size() == mesh.positions.size())
      base.normal = EiemAddGltfAccessor(accessors, views, blob, mesh.normals.data(),
                                        sizeof(Vector3), mesh.normals.size(), 5126,
                                        "VEC3", 34962);
    if (mesh.tangents.size() == mesh.positions.size())
      base.tangent = EiemAddGltfAccessor(accessors, views, blob, mesh.tangents.data(),
                                         sizeof(Vector4), mesh.tangents.size(), 5126,
                                         "VEC4", 34962);
    if (mesh.uv.size() == mesh.positions.size())
      base.uv = EiemAddGltfAccessor(accessors, views, blob, mesh.uv.data(),
                                    sizeof(Vector2), mesh.uv.size(), 5126, "VEC2",
                                    34962);
    if (mesh.colors.size() == mesh.positions.size() * 4)
      base.color = EiemAddGltfAccessor(accessors, views, blob, mesh.colors.data(),
                                       sizeof(float) * 4, mesh.positions.size(),
                                       5126, "VEC4", 34962);
    if (mesh.boneWeights.size() == mesh.positions.size()) {
      std::vector<uint16_t> joints(mesh.boneWeights.size() * 4);
      std::vector<float> weights(mesh.boneWeights.size() * 4);
      for (size_t i = 0; i < mesh.boneWeights.size(); ++i) {
        const BoneWeight &bw = mesh.boneWeights[i];
        joints[i * 4 + 0] = (uint16_t)(bw.boneIndex0 < 0 ? 0 : bw.boneIndex0);
        joints[i * 4 + 1] = (uint16_t)(bw.boneIndex1 < 0 ? 0 : bw.boneIndex1);
        joints[i * 4 + 2] = (uint16_t)(bw.boneIndex2 < 0 ? 0 : bw.boneIndex2);
        joints[i * 4 + 3] = (uint16_t)(bw.boneIndex3 < 0 ? 0 : bw.boneIndex3);
        weights[i * 4 + 0] = bw.weight0;
        weights[i * 4 + 1] = bw.weight1;
        weights[i * 4 + 2] = bw.weight2;
        weights[i * 4 + 3] = bw.weight3;
      }
      base.joints = EiemAddGltfAccessor(accessors, views, blob, joints.data(),
                                        sizeof(uint16_t) * 4, mesh.positions.size(),
                                        5123, "VEC4", 34962);
      base.weights = EiemAddGltfAccessor(accessors, views, blob, weights.data(),
                                         sizeof(float) * 4, mesh.positions.size(),
                                         5126, "VEC4", 34962);
    }
    const size_t primitiveCount = mesh.submeshIndices.empty()
                                      ? 1
                                      : mesh.submeshIndices.size();
    for (size_t primitiveIndex = 0; primitiveIndex < primitiveCount;
         ++primitiveIndex) {
      const std::vector<uint32_t> &indices =
          mesh.submeshIndices.empty() ? mesh.indices
                                      : mesh.submeshIndices[primitiveIndex];
      if (indices.empty()) continue;
      Primitive p = base;
      p.indices = EiemAddGltfAccessor(accessors, views, blob, indices.data(),
                                      sizeof(uint32_t), indices.size(),
                                      5125, "SCALAR", 34963);
      const size_t materialSlot =
          primitiveIndex < mesh.materials.size() ? primitiveIndex : 0;
      if (!mesh.materials.empty()) {
        p.material = 0;
        for (size_t materialIndex = 0; materialIndex < gltfMaterials.size();
             ++materialIndex) {
          if (gltfMaterials[materialIndex] == mesh.materials[materialSlot]) {
            p.material = (int)materialIndex;
            break;
          }
        }
      }
      info.primitives.push_back(p);
    }
    gltfMeshes.push_back(std::move(info));
  }

  FILE *bin = nullptr;
  char binPath[768] = {};
  snprintf(binPath, sizeof(binPath), "%s\\model.bin", directory);
  bin = fopen(binPath, "wb");
  if (!bin) return;
  if (!blob.bytes.empty()) fwrite(blob.bytes.data(), 1, blob.bytes.size(), bin);
  fclose(bin);

  char gltfPath[768] = {};
  snprintf(gltfPath, sizeof(gltfPath), "%s\\model.gltf", directory);
  FILE *json = fopen(gltfPath, "wb");
  if (!json) return;
  fprintf(json, "{\n  \"asset\":{\"version\":\"2.0\",\"generator\":\"EFF\"},\n");
  fprintf(json, "  \"buffers\":[{\"uri\":\"model.bin\",\"byteLength\":%zu}],\n", blob.bytes.size());
  fprintf(json, "  \"bufferViews\":[");
  for (size_t i = 0; i < views.size(); ++i) {
    if (i) fputc(',', json);
    fprintf(json, "{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu,\"target\":%d}",
            views[i].offset, views[i].length, views[i].target);
  }
  fprintf(json, "],\n  \"accessors\":[");
  for (size_t i = 0; i < accessors.size(); ++i) {
    if (i) fputc(',', json);
    fprintf(json, "{\"bufferView\":%d,\"byteOffset\":%zu,\"componentType\":%d,\"count\":%zu,\"type\":\"%s\"%s}",
            accessors[i].view, accessors[i].offset, accessors[i].componentType,
            accessors[i].count, accessors[i].type,
            accessors[i].normalized ? ",\"normalized\":true" : "");
  }
  fprintf(json, "],\n  \"materials\":[");
  for (size_t i = 0; i < gltfMaterials.size(); ++i) {
    if (i) fputc(',', json);
    fprintf(json, "{\"name\":");
    EiemWriteJsonString(json, gltfMaterials[i].c_str());
    fprintf(json, "}");
  }
  fprintf(json, "],\n  \"meshes\":[");
  for (size_t mi = 0; mi < gltfMeshes.size(); ++mi) {
    if (mi) fputc(',', json);
    fprintf(json, "{\"name\":");
    EiemWriteJsonString(json, gltfMeshes[mi].name.c_str());
    fprintf(json, ",\"primitives\":[");
    for (size_t pi = 0; pi < gltfMeshes[mi].primitives.size(); ++pi) {
      if (pi) fputc(',', json);
      const Primitive &p = gltfMeshes[mi].primitives[pi];
      fprintf(json, "{\"attributes\":{\"POSITION\":%d", p.position);
      if (p.normal >= 0) fprintf(json, ",\"NORMAL\":%d", p.normal);
      if (p.tangent >= 0) fprintf(json, ",\"TANGENT\":%d", p.tangent);
      if (p.uv >= 0) fprintf(json, ",\"TEXCOORD_0\":%d", p.uv);
      if (p.color >= 0) fprintf(json, ",\"COLOR_0\":%d", p.color);
      if (p.joints >= 0) fprintf(json, ",\"JOINTS_0\":%d,\"WEIGHTS_0\":%d", p.joints, p.weights);
      fprintf(json, "},\"indices\":%d", p.indices);
      if (p.material >= 0) fprintf(json, ",\"material\":%d", p.material);
      fprintf(json, "}");
    }
    fprintf(json, "]}");
  }
  fprintf(json, "],\n  \"nodes\":[");
  for (size_t i = 0; i < gltfMeshes.size(); ++i) {
    if (i) fputc(',', json);
    fprintf(json, "{\"name\":");
    EiemWriteJsonString(json, gltfMeshes[i].name.c_str());
    fprintf(json, ",\"mesh\":%zu}", i);
  }
  fprintf(json, "],\n  \"scenes\":[{\"nodes\":[");
  for (size_t i = 0; i < gltfMeshes.size(); ++i) {
    if (i) fputc(',', json);
    fprintf(json, "%zu", i);
  }
  fprintf(json, "]}],\"scene\":0\n}\n");
  fclose(json);

  char metaPath[768] = {};
  snprintf(metaPath, sizeof(metaPath), "%s\\eff.json", directory);
  FILE *meta = fopen(metaPath, "wb");
  if (!meta) return;
  fprintf(meta, "{\n  \"schema\":1,\n  \"format\":\"gltf2\",\n  \"model\":\"model.gltf\",\n  \"meshes\":[\n");
  for (size_t i = 0; i < meshes.size(); ++i) {
    if (i) fputs(",\n", meta);
    fprintf(meta, "    {\"name\":");
    EiemWriteJsonString(meta, meshes[i].name.c_str());
    fprintf(meta, ",\"hierarchy\":");
    EiemWriteJsonString(meta, meshes[i].hierarchy.c_str());
    fprintf(meta, ",\"identity\":");
    EiemWriteJsonString(meta, meshes[i].identity.c_str());
    fprintf(meta, ",\"identityHash\":\"%016llx\"",
            EiemIdentityHash(meshes[i].identity));
    fprintf(meta, ",\"rendererPath\":");
    EiemWriteJsonString(meta, meshes[i].rendererPath.c_str());
    fprintf(meta, ",\"lod\":%d,\"vertexCount\":%zu,\"indexCount\":%zu,\"bindposeCount\":%zu,\"bones\":[",
            meshes[i].lod,
            meshes[i].positions.size(), meshes[i].indices.size(),
            meshes[i].bindposes.size());
    for (size_t b = 0; b < meshes[i].bones.size(); ++b) {
      if (b) fputc(',', meta);
      EiemWriteJsonString(meta, meshes[i].bones[b].c_str());
    }
    fprintf(meta, "],\"rootBone\":");
    EiemWriteJsonString(meta, meshes[i].rootBone.c_str());
    fprintf(meta, ",\"bindposes\":[");
    for (size_t b = 0; b < meshes[i].bindposes.size(); ++b) {
      if (b) fputc(',', meta);
      fputc('[', meta);
      for (size_t v = 0; v < 16; ++v) {
        if (v) fputc(',', meta);
        fprintf(meta, "%.9g", meshes[i].bindposes[b].m[v]);
      }
      fputc(']', meta);
    }
    fprintf(meta, "],\"blendShapes\":[");
    for (size_t b = 0; b < meshes[i].blendShapes.size(); ++b) {
      if (b) fputc(',', meta);
      fprintf(meta, "{\"name\":");
      EiemWriteJsonString(meta, meshes[i].blendShapes[b].c_str());
      fprintf(meta, ",\"weight\":%.9g}",
              b < meshes[i].blendShapeWeights.size()
                  ? meshes[i].blendShapeWeights[b]
                  : 0.0f);
    }
    fprintf(meta, "],\"materials\":[");
    for (size_t m = 0; m < meshes[i].materials.size(); ++m) {
      if (m) fputc(',', meta);
      fprintf(meta, "{\"slot\":%zu,\"name\":", m);
      EiemWriteJsonString(meta, meshes[i].materials[m].c_str());
      const EiemMaterialExport *detail =
          m < meshes[i].materialDetails.size()
              ? &meshes[i].materialDetails[m]
              : nullptr;
      fprintf(meta, ",\"shader\":");
      EiemWriteJsonString(meta, detail ? detail->shader.c_str() : "<unresolved>");
      fprintf(meta, ",\"textures\":[");
      if (detail) {
        const size_t textureCount = detail->textureDetails.empty()
                                        ? detail->textures.size()
                                        : detail->textureDetails.size();
        for (size_t t = 0; t < textureCount; ++t) {
          if (t) fputc(',', meta);
          fprintf(meta, "{\"property\":");
          const std::string &property = detail->textureDetails.empty()
                                            ? detail->textures[t].first
                                            : detail->textureDetails[t].property;
          const std::string &textureName = detail->textureDetails.empty()
                                                ? detail->textures[t].second
                                                : detail->textureDetails[t].name;
          EiemWriteJsonString(meta, property.c_str());
          fprintf(meta, ",\"name\":");
          EiemWriteJsonString(meta, textureName.c_str());
          fprintf(meta, ",\"width\":%d,\"height\":%d}",
                  detail->textureDetails.empty() ? 0 : detail->textureDetails[t].width,
                  detail->textureDetails.empty() ? 0 : detail->textureDetails[t].height);
        }
      }
      fprintf(meta, "],\"properties\":[");
      if (detail) {
        for (size_t p = 0; p < detail->properties.size(); ++p) {
          if (p) fputc(',', meta);
          fprintf(meta, "{\"name\":");
          EiemWriteJsonString(meta, detail->properties[p].first.c_str());
          fprintf(meta, ",\"value\":");
          EiemWriteJsonString(meta, detail->properties[p].second.c_str());
          fprintf(meta, "}");
        }
      }
      fprintf(meta, "]}");
    }
    fprintf(meta, "],\"source\":{");
    if (i < observations.size()) {
      fprintf(meta, "\"rendererType\":");
      EiemWriteJsonString(meta, observations[i].rendererType);
      fprintf(meta, ",\"renderer\":");
      EiemWriteJsonString(meta, observations[i].rendererName);
      fprintf(meta, ",\"instanceCount\":%u,\"nativeMesh\":\"%p\",\"nativeRenderer\":\"%p\"",
              observations[i].instanceCount, observations[i].mesh,
              observations[i].renderer);
    }
    fprintf(meta, "}}");
  }
  fprintf(meta, "\n  ],\n  \"sourceObservations\":%zu,\n  \"textures\":[],\n  \"physics\":[]\n}\n",
          observations.size());
  fclose(meta);
}

static void EiemExportModels(bool full) {
  if (InterlockedCompareExchange(&s_dumpInProgress, 1, 0) != 0) {
    EiemSetDumpStatus("Export already in progress");
    return;
  }
  const size_t count = TraceCopyMeshObservations(
      s_dumpGameSnapshot, _countof(s_dumpGameSnapshot));
  void **selected = s_dumpSelectedSnapshot;
  size_t selectedCount = 0;
  EiemCopySelection(selected, _countof(s_dumpSelectedSnapshot), &selectedCount);
  std::vector<EiemExportMesh> meshes;
  std::vector<EiemMeshObservation> sources;
  for (size_t i = 0; i < count; ++i) {
    if (!full && !EiemPointerInList(s_dumpGameSnapshot[i].mesh, selected,
                                    selectedCount))
      continue;
    EiemExportMesh exported;
    if (EiemReadMeshExport(s_dumpGameSnapshot[i].mesh,
                           s_dumpGameSnapshot[i].renderer,
                           s_dumpGameSnapshot[i], exported)) {
      meshes.push_back(std::move(exported));
      sources.push_back(s_dumpGameSnapshot[i]);
    }
  }
  if (meshes.empty()) {
    EiemSetDumpStatus("No readable Mesh selected");
    InterlockedExchange(&s_dumpInProgress, 0);
    return;
  }
  char directory[768] = {};
  snprintf(directory, sizeof(directory), "%s\\model_%llu", g_dumpOutputDir,
           (unsigned long long)GetTickCount64());
  if (!EiemEnsureDirectory(directory)) {
    EiemSetDumpStatus("Cannot create model output directory");
    InterlockedExchange(&s_dumpInProgress, 0);
    return;
  }
  EiemWriteGltf(directory, meshes, sources);
  char status[256] = {};
  snprintf(status, sizeof(status), "Exported %zu Mesh(es) to %s", meshes.size(),
           directory);
  EiemSetDumpStatus(status);
  Log("[MODEL-DUMP] %s", status);
  InterlockedExchange(&s_dumpInProgress, 0);
}
