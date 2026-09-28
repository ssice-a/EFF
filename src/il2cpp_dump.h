#pragma once

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

// Metadata-only discovery pass used to identify the game's resource pipeline.
// It does not invoke managed code or touch Unity objects, so it is safe to run
// immediately after il2cpp_domain_get_assemblies().

static std::string Il2CppLowerAscii(const char *value) {
  if (!value) return {};
  std::string result(value);
  for (char &ch : result)
    ch = (char)tolower((unsigned char)ch);
  return result;
}

static bool Il2CppContainsAny(const std::string &value,
                              const char *const *terms, int termCount) {
  for (int i = 0; i < termCount; i++) {
    if (value.find(terms[i]) != std::string::npos) return true;
  }
  return false;
}

static bool IsIl2CppResourceCandidate(const char *ns, const char *name) {
  static const char *const terms[] = {
      "assetbundle", "addressable", "resource", "resourceprovider",
      "resourcemanager", "catalog", "assetreference", "prefab", "model",
      "mesh", "renderer", "material", "gameobject", "component",
      "transform", "animator", "avatar", "character", "entity", "visual",
      "vfs", "virtualfilesystem", "filesystem", "lowio", "filehandle",
      "stringpathhash", "mappingstr", "manifestbinary",
  };
  const std::string full = Il2CppLowerAscii(ns) + "." +
                           Il2CppLowerAscii(name);
  if (Il2CppContainsAny(full, terms, (int)(sizeof(terms) / sizeof(terms[0]))))
    return true;

  // Keep Unity's resource-related namespaces even when a class has a generic
  // name such as ResourceLocation or AsyncOperationHandle.
  const std::string lowerNs = Il2CppLowerAscii(ns);
  return lowerNs.find("unityengine.resourcemanagement") != std::string::npos ||
         lowerNs.find("unityengine.addressableassets") != std::string::npos;
}

static const char *Il2CppTypeName(void *type) {
  if (!type || !il2cpp_type_get_name) return "?";
  const char *name = nullptr;
  __try { name = il2cpp_type_get_name(type); }
  __except (1) { name = nullptr; }
  return name ? name : "?";
}

static void *Il2CppMethodPointer(void *method) {
  if (!method) return nullptr;
  void *pointer = nullptr;
  __try { pointer = ((MInfo *)method)->mp; }
  __except (1) { pointer = nullptr; }
  return pointer;
}

static void DumpIl2CppMethod(FILE *out, void *method) {
  if (!out || !method || !il2cpp_method_get_name ||
      !il2cpp_method_get_param_count)
    return;

  const char *name = nullptr;
  uint32_t paramCount = 0;
  __try {
    name = il2cpp_method_get_name(method);
    paramCount = il2cpp_method_get_param_count(method);
  } __except (1) {
    return;
  }
  if (!name) name = "?";

  const char *returnType = "?";
  if (il2cpp_method_get_return_type) {
    void *returnTypeObject = nullptr;
    __try { returnTypeObject = il2cpp_method_get_return_type(method); }
    __except (1) { returnTypeObject = nullptr; }
    returnType = Il2CppTypeName(returnTypeObject);
  }

  fprintf(out, "  METHOD method=%p mp=%p %s %s(", method,
          Il2CppMethodPointer(method), returnType, name);
  for (uint32_t i = 0; i < paramCount; i++) {
    if (i) fprintf(out, ", ");
    void *paramType = nullptr;
    if (il2cpp_method_get_param) {
      __try { paramType = il2cpp_method_get_param(method, i); }
      __except (1) { paramType = nullptr; }
    }
    fprintf(out, "%s", Il2CppTypeName(paramType));
  }
  fprintf(out, ")\n");
}

static void DumpIl2CppClassDetails(FILE *out, void *klass,
                                   const char *ns, const char *name) {
  if (!out || !klass) return;
  fprintf(out, "\nCLASS %s.%s @%p\n", ns ? ns : "", name ? name : "?", klass);

  if (il2cpp_class_get_parent) {
    void *parent = nullptr;
    __try { parent = il2cpp_class_get_parent(klass); }
    __except (1) { parent = nullptr; }
    if (parent) {
      const char *parentNs = il2cpp_class_get_namespace
                                 ? il2cpp_class_get_namespace(parent)
                                 : "";
      const char *parentName = il2cpp_class_get_name
                                   ? il2cpp_class_get_name(parent)
                                   : "?";
      fprintf(out, "  PARENT %s.%s\n", parentNs ? parentNs : "",
              parentName ? parentName : "?");
    }
  }

  fprintf(out, "  FIELDS\n");
  if (il2cpp_class_get_fields) {
    void *iter = nullptr;
    void *field = nullptr;
    while (true) {
      __try { field = il2cpp_class_get_fields(klass, &iter); }
      __except (1) { field = nullptr; }
      if (!field) break;

      const char *fieldName = "?";
      if (il2cpp_field_get_name) {
        __try { fieldName = il2cpp_field_get_name(field); }
        __except (1) { fieldName = "?"; }
      }
      size_t offset = 0;
      if (il2cpp_field_get_offset) {
        __try { offset = il2cpp_field_get_offset(field); }
        __except (1) { offset = 0; }
      }
      int flags = 0;
      if (il2cpp_field_get_flags) {
        __try { flags = il2cpp_field_get_flags(field); }
        __except (1) { flags = 0; }
      }
      void *fieldType = nullptr;
      if (il2cpp_field_get_type) {
        __try { fieldType = il2cpp_field_get_type(field); }
        __except (1) { fieldType = nullptr; }
      }
      fprintf(out, "    FIELD offset=0x%zX flags=0x%X type=%s %s\n", offset,
              flags, Il2CppTypeName(fieldType), fieldName ? fieldName : "?");
    }
  }

  fprintf(out, "  METHODS\n");
  if (il2cpp_class_get_methods) {
    void *iter = nullptr;
    void *method = nullptr;
    while (true) {
      __try { method = il2cpp_class_get_methods(klass, &iter); }
      __except (1) { method = nullptr; }
      if (!method) break;
      DumpIl2CppMethod(out, method);
    }
  }
}

static void DumpIl2CppMetadata(void **assemblies, size_t assemblyCount) {
  if (!assemblies || assemblyCount == 0) return;
  CreateDirectoryA("plugin", nullptr);

  FILE *classes = fopen("plugin\\eff_il2cpp_classes.txt", "w");
  FILE *details = fopen("plugin\\eff_il2cpp_resource_dump.txt", "w");
  if (!classes || !details) {
    if (classes) fclose(classes);
    if (details) fclose(details);
    Log("[IL2CPP-DUMP] Cannot create dump files");
    return;
  }

  fprintf(classes, "=== EFF IL2CPP CLASS INDEX ===\n");
  fprintf(classes, "Assemblies: %zu\n\n", assemblyCount);
  fprintf(details, "=== EFF IL2CPP RESOURCE CANDIDATES ===\n");
  fprintf(details, "This file contains metadata only; no managed methods were invoked.\n");

  size_t totalClasses = 0;
  size_t candidateClasses = 0;
  for (size_t ai = 0; ai < assemblyCount; ai++) {
    void *image = nullptr;
    __try { image = il2cpp_assembly_get_image(assemblies[ai]); }
    __except (1) { image = nullptr; }
    if (!image) continue;

    const char *imageName = il2cpp_image_get_name
                                ? il2cpp_image_get_name(image)
                                : "?";
    size_t classCount = 0;
    if (il2cpp_image_get_class_count) {
      __try { classCount = il2cpp_image_get_class_count(image); }
      __except (1) { classCount = 0; }
    }
    fprintf(classes, "ASSEMBLY %s classes=%zu\n", imageName ? imageName : "?",
            classCount);

    for (size_t ci = 0; ci < classCount; ci++) {
      void *klass = nullptr;
      __try { klass = il2cpp_image_get_class(image, ci); }
      __except (1) { klass = nullptr; }
      if (!klass) continue;
      totalClasses++;

      const char *ns = il2cpp_class_get_namespace
                           ? il2cpp_class_get_namespace(klass)
                           : "";
      const char *name = il2cpp_class_get_name
                             ? il2cpp_class_get_name(klass)
                             : "?";
      fprintf(classes, "  %s.%s @%p\n", ns ? ns : "", name ? name : "?",
              klass);

      if (IsIl2CppResourceCandidate(ns, name)) {
        candidateClasses++;
        fprintf(details, "\nASSEMBLY %s\n", imageName ? imageName : "?");
        DumpIl2CppClassDetails(details, klass, ns, name);
      }
    }
    fprintf(classes, "\n");
  }

  fprintf(classes, "SUMMARY classes=%zu\n", totalClasses);
  fprintf(details, "\nSUMMARY candidates=%zu total_classes=%zu\n",
          candidateClasses, totalClasses);
  fclose(classes);
  fclose(details);

  Log("[IL2CPP-DUMP] Wrote %zu classes and %zu resource candidates",
      totalClasses, candidateClasses);
  Log("[IL2CPP-DUMP] See plugin\\eff_il2cpp_classes.txt and "
      "plugin\\eff_il2cpp_resource_dump.txt");
}
