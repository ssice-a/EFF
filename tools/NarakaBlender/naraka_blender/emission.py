"""Shared dispatch for the verified RG/BA periodic-emission contract."""
from .advanced_cloth import VERIFIED_SHADERS


STANDARD_PHASE_SHADERS = {
    "lx22standardcharactercloth.shader",
    "lx22standardcharacterclothsilk.shader",
}
PERIODIC2_KEYWORDS = {"_MFX_EMISSIVE_PERIODIC_2", "_MFX_EMISSIVE_PERIODIC_2_ON"}


def phase_cloth_shader(shader):
    shader = shader.lower().replace("\\", "/").rsplit("/", 1)[-1]
    return shader in VERIFIED_SHADERS | STANDARD_PHASE_SHADERS or "periodic2" in shader


def periodic2_enabled(shader, floats, keywords):
    """A supported shader AND its enabled variant are both required.

    The standard cloth/silk variants use the same two packed waves as the
    specialized graphs; their shader filenames do not contain 'periodic2'.
    """
    return phase_cloth_shader(shader) and bool(
        floats.get("_MFX_EMISSIVE_PERIODIC_2", 0) or set(keywords) & PERIODIC2_KEYWORDS)
