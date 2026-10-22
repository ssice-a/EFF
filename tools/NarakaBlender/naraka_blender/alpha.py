"""Material alpha contracts; independent of image contents or image file names.

Keep policy in step with NarakaPreviewMaterialState; shared regression fixtures
exercise both implementations. Unknown shader alpha stays available as data.
"""


def resolve_alpha(source):
    shader = source.get("shader", "").replace("\\", "/").rsplit("/", 1)[-1].lower()
    floats = source.get("floats", {})
    tokens = set(source.get("keywords", "").split())
    periodic = shader == "ch_cloth_silk_specular4_periodic2.shadergraph"
    silk = shader == "lx22standardcharacterclothsilk.shader"
    opaque_graph = shader == "ch_cloth_silk_specular4_periodic2_opaque.shadergraph"
    depth_only = shader == "lx22transparentanimdepth.shader"
    known_opaque = shader in {"lx22standardcharacterskin.shader", "lx22standardcharacter.shader", "lx22standardcharactercloth.shader"}
    clip_switch = (
        bool(tokens & {"_USE_CUTOUT", "_ALPHATEST_ON"}) or
        floats.get("_AlphaClip") == 1 or floats.get("_AlphaTest") == 1)
    cutout = not (depth_only or opaque_graph) and ("_USE_CUTOUT" in tokens if known_opaque else clip_switch)
    explicit_blend = floats["_Surface"] == 1 if "_Surface" in floats else (
        "_ALPHABLEND_ON" in tokens or (floats.get("_SrcBlend") == 5 and floats.get("_DstBlend") == 10))
    blend = not (depth_only or opaque_graph or known_opaque) and (periodic or silk or explicit_blend)
    # Explicit blend state still does not identify the fragment alpha's source.
    resolved = depth_only or opaque_graph or periodic or silk or known_opaque
    use_base_alpha = resolved and not depth_only and (cutout or blend)
    reason = ("unresolved-shader-alpha-unused" if not resolved else "dedicated-depth" if depth_only else "opaque-shader-profile" if opaque_graph else
        "silk-transparent-pass" if periodic or silk else "explicit-material-alpha-state" if cutout or blend else
        "opaque-material-state")
    return dict(cutout=cutout, blend=blend, silkCoverage=periodic or silk, depthOnly=depth_only,
        resolved=resolved, useBaseAlpha=use_base_alpha, reason=reason, cutoff=floats.get("_Cutoff", 0.5) if silk else
            floats.get("_AlphaClipThreshold", floats.get("_Cutoff", 0.5)),
        cull=int(floats.get("_Cull", floats.get("_Culling", 2))))
