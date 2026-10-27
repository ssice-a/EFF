#pragma once

// The current writer has one self-described package layout. Historical v35/v36
// markers remain accepted by readers; they do not select separate builds.
namespace EiemPackageProtocol {
constexpr const char* shapeAnimation = "sampled-shape-weight-clips/le32/1";
constexpr const char* shapeAnimationPlayback = "shared-channel-groups/replace-conflicting-playback";
constexpr const char* materialAnimation = "sampled-material-property-clips/renderer-slot-block/le32/1";
constexpr const char* effectAttachment = "isolated-native-effect/mesh-owner+bone/le32/1";
constexpr const char* effectTarget = "source-prefab+renderer+mesh+bone-palette/strings/1";
constexpr const char* standaloneEffect = "mod-ini/type=effect/independent-resource-graph/1";
constexpr const char* manifestStructure = "author-graph+inputs+roots+closure+parts+materials+key-label/le64/sha256";
constexpr const char* authorStructure = "mod-ini/parts+replace+resources+controls+key-label";
constexpr const char* resourceStructure = "block-directory/le64/sha256/type-tree";
constexpr const char* currentStructure = "key+manifest-sha256/tsv";
constexpr const char* setStructure = "authors+shared-inputs/transaction";
constexpr const char* staticSourceInputs = "mod-static-catalog/selected-sources";
constexpr const char* ruleStructure = "author-references+prefab-renderer-rules+source-ranges+semantic-deltas+key-label/le64/sha256";
constexpr const char* semanticRendererReplay = "logical-mesh+field-path-overlay/current-typetree/1";
constexpr const char* ruleEncoding = "compressed-rules/xpress-huffman/le64";
constexpr const char* compiledObjects = "serialized-records/add+edit+delete+native-types+scripts+pptr-remap";
constexpr const char* resourcePayloads = "mod-ini/resource+block-directory/opaque-kind";
constexpr const char* compiledInputView = "native-encoded-block-spans+literal-name-sites/le64";
constexpr const char* compiledRuntimeIndex = "native-view-index+assembly-dictionary+verified-source-stamps+per-input-proofs/le64/sha256";
constexpr const char* compiledModStore = "EFF_MOD_BLOCKS/2";
constexpr const char* resourceUpdate = "addressed-resource-rebind/1";
constexpr unsigned authorFormat = 2;
constexpr unsigned resourceVersion = 2;
constexpr bool rootAssembly = true;
constexpr unsigned abi = 35;
constexpr const char* compiler = "EFF_NATIVE_PACK_V35_1";
} // namespace EiemPackageProtocol
