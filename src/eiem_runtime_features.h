#pragma once

static constexpr bool kEiemEnableExperimentalPhysicsRuntime = false;
static constexpr bool kEiemEnableNativePhysicsObservation = false;

static constexpr bool kEiemEnableContinuousMeshObservation = false;
static constexpr bool kEiemEnableLifecycleDiagnostics = false;
#if defined(EIEM_NATIVE_BOUNDARY_STACKS_BUILD)
static constexpr bool kEiemEnableNativeBoundaryStacks = true;
#else
static constexpr bool kEiemEnableNativeBoundaryStacks = false;
#endif
static constexpr bool kEiemEnableSkinDiagnostics = false;
static constexpr bool kEiemEnableNativeMeshBoneSlots = true;
#if defined(EIEM_NATIVE_MESH_BONE_SLOTS_PROBE_BUILD)
static constexpr bool kEiemEnableSkinBindingDiagnostics = true;
static constexpr bool kEiemEnableNativeMeshFlagProbe = true;
#else
static constexpr bool kEiemEnableSkinBindingDiagnostics = false;
static constexpr bool kEiemEnableNativeMeshFlagProbe = false;
#endif
static constexpr bool kEiemEnableSkinTimingProbe = false;
#if defined(EIEM_PFB_MESH_BOUNDARY_TRACE_BUILD)
static constexpr bool kEiemEnableDescriptorDiagnostics = true;
#else
static constexpr bool kEiemEnableDescriptorDiagnostics = false;
#endif
static constexpr bool kEiemEnableCustomSkinPipelineMetadata = false;
static constexpr bool kEiemEnableCustomSkinPipelineObservation = false;
