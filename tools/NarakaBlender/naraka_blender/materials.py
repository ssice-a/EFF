"""Editable Blender material adapters; original Unity parameters stay intact.

This is a preview conversion, not execution of the game's compiled shaders.
Unconverted features and assumptions are recorded per import and per material.
"""
import json
import math

import bpy

from .format import safe_path
from .layout import arrange_material
from .alpha import resolve_alpha


class MaterialBuilder:
    def __init__(self, root, manifest, report, readable=True, profile="LIGHTWEIGHT"):
        self.root, self.manifest, self.report = root, manifest, report
        self.readable = readable
        self.profile = profile
        self.images, self.adapters, self.statuses = {}, {}, {}

    def image(self, identity, linear, slice_index=None):
        key = (identity, linear, slice_index)
        if key not in self.images:
            source = self.manifest["textures"][identity]
            path = source["slices"][slice_index] if slice_index is not None else source["path"]
            image = bpy.data.images.load(str(safe_path(self.root, path)), check_existing=False)
            image.name = source["name"] + (" [%d]" % slice_index if slice_index is not None else "") + (" [data]" if linear else " [color]")
            image.colorspace_settings.name = "Non-Color" if linear else "sRGB"
            # Shader nodes consume RGB and A independently. In particular a
            # data-bearing diffuse A must not premultiply away its base color.
            image.alpha_mode = "CHANNEL_PACKED"
            image["naraka_source"] = identity
            image["naraka_source_linear"] = source["linear"]
            if len(image.pixels) == 0:
                raise ValueError("Texture pixels unavailable: " + source["path"])
            self.images[key] = image
        return self.images[key]

    def build(self, source):
        if self.profile == "LIGHTWEIGHT":
            from .lightweight import LightweightGraph
            return LightweightGraph(self, source).build()
        return Graph(self, source).build()


class Graph:
    def __init__(self, builder, source):
        self.builder, self.source = builder, source
        self.material = bpy.data.materials.new(source["name"])
        self.material.use_nodes = True
        self.nodes = self.material.node_tree.nodes
        self.links = self.material.node_tree.links
        self.nodes.clear()
        declarations = source.get("shaderProperties", {})
        self.floats = {k: v["defaults"][0] for k, v in declarations.items() if v["type"] in ("Float", "Range", "Int")}
        self.vectors = {k: v["defaults"] for k, v in declarations.items() if v["type"] in ("Vector", "Color")}
        self.floats.update(source["floats"])
        self.vectors.update(source["vectors"])
        self.keywords = set(source.get("keywords", "").split())
        self.textures = source["textures"]
        self.notes, self.used, self.verified = [], set(), []
        self.sampled = {}
        self.material["naraka_source"] = source["id"]
        self.material["naraka_shader"] = source["shader"]
        self.material["naraka_parameters_json"] = json.dumps(source, ensure_ascii=False)
        self.section = "time"
        self.time = self.node("ShaderNodeValue", "Time in seconds", -1400, 700)
        self.time.outputs[0].default_value = 0
        driver = self.time.outputs[0].driver_add("default_value").driver
        for name, data_path in (("fps", "render.fps"), ("base", "render.fps_base")):
            var = driver.variables.new()
            var.name, var.type = name, "SINGLE_PROP"
            var.targets[0].id_type = "SCENE"
            var.targets[0].id = bpy.context.scene
            var.targets[0].data_path = data_path
        driver.expression = "frame*base/fps"

    def node(self, kind, label, x=-600, y=0):
        node = self.nodes.new(kind)
        node.label = label
        node["naraka_section"] = self.section
        node.location = (x, y)
        return node

    def assign(self, socket, value):
        if hasattr(value, "is_output"):
            self.links.new(value, socket)
        else:
            if isinstance(value, (float, int)) and socket.type in ("RGBA", "VECTOR"):
                value = (value, value, value, 1) if socket.type == "RGBA" else (value, value, value)
            socket.default_value = value

    def math(self, operation, a, b=0, label="", x=-300, y=0):
        node = self.node("ShaderNodeMath", label or operation, x, y)
        node.operation = operation
        self.assign(node.inputs[0], a)
        self.assign(node.inputs[1], b)
        return node.outputs[0]

    def mix(self, a, b, factor, label):
        node = self.node("ShaderNodeMixRGB", label)
        self.assign(node.inputs[0], factor)
        self.assign(node.inputs[1], a); self.assign(node.inputs[2], b)
        return node.outputs[0]

    def multiply_color(self, color, tint, label):
        node = self.node("ShaderNodeMixRGB", label, -100, 150)
        node.blend_type = "MULTIPLY"
        node.inputs[0].default_value = 1
        self.assign(node.inputs[1], color)
        self.assign(node.inputs[2], tint)
        return node.outputs[0]

    def add_color(self, a, b, label):
        node = self.node("ShaderNodeMixRGB", label, 350, 450)
        node.blend_type = "ADD"; node.inputs[0].default_value = 1
        self.assign(node.inputs[1], a); self.assign(node.inputs[2], b)
        return node.outputs[0]

    def saturate(self, value, minimum=0, maximum=1):
        return self.math("MINIMUM", self.math("MAXIMUM", value, minimum), maximum)

    def periodic_emission(self, texture, normal, advanced=False):
        """Two explicit portable phase-wave groups; source UV stays fixed."""
        from .readable import wave_tree
        texture.label = "流光数据 · R/B 区域，G/A 相位"
        texture["naraka_keep_root"] = True
        texture["naraka_role"] = "flow_texture"
        self.section = "emission"
        channels = self.separate(texture)
        channels[0].node.label = "通道分工 · R→层1区域，G→层1相位，B→层2区域"
        channels[0].node["naraka_keep_root"] = True
        channels[0].node["naraka_role"] = "flow_channels"
        masks = (channels[0], channels[2])
        phases = (channels[1], texture.outputs["Alpha"])
        emission = self.vectors.get("_EmissionColor", [1, 1, 1, 1])
        color = self.vectors.get("_MatEffectColor0", [0, 0, 0, 0])
        extra = self.vectors.get("_MatEffectParam2", [1, 0, 16, 16])
        geometry = self.node("ShaderNodeNewGeometry", "视角和表面方向")
        dot = self.node("ShaderNodeVectorMath", "Normal · view")
        dot.operation = "DOT_PRODUCT"
        self.links.new(normal or geometry.outputs["Normal"], dot.inputs[0])
        self.links.new(geometry.outputs["Incoming"], dot.inputs[1])
        facing = dot.outputs["Value"]
        if advanced or self.floats.get("_MatEffectReverseUniform", 0):
            facing = self.math("ABSOLUTE", facing)
        terms = []
        for i in range(2):
            self.section = "flow"
            node = self.node("ShaderNodeGroup", "流光 %d · %s区域 / %s相位" % (i+1, "RB"[i], "GA"[i]))
            node.node_tree = wave_tree()
            node["naraka_role"] = "flow_layer"
            node["naraka_flow_layer"] = i
            param = self.vectors.get("_MatEffectParam%d" % i, [1, 0, 0, 1])
            tint = emission if i == 0 else [color[j] * (1 + extra[1] * (emission[j] - 1)) for j in range(3)] + [1]
            for name, value in {
                "区域蒙版": masks[i], "相位": phases[i], "时间 秒": self.time.outputs[0],
                "相位频率": param[0], "速度 rad/s": param[1], "阈值": param[2], "锐度": param[3],
                "视角点积": facing, "视角参数": extra[i+2], "颜色": tint, "强度": 1,
            }.items():
                self.assign(node.inputs[name], value)
            terms.append(node.outputs["发光"])
        self.section = "emission"
        back_dot = dot.outputs["Value"]
        if advanced:
            geometric_dot = self.node("ShaderNodeVectorMath", "几何面方向 · 决定背面强度")
            geometric_dot.operation = "DOT_PRODUCT"
            self.links.new(geometry.outputs["Normal"], geometric_dot.inputs[0])
            self.links.new(geometry.outputs["Incoming"], geometric_dot.inputs[1])
            back_dot = geometric_dot.outputs["Value"]
        negative = self.math("LESS_THAN", back_dot, 0)
        factor = self.math("ADD", 1, self.math("MULTIPLY", negative, color[3] - 1))
        self.section = "emission_mix"
        combined = self.add_color(*terms, "两层流光相加")
        self.notes.append("Periodic2 uses fixed UV and animated phase: phase*frequency + timeSeconds*speed; two editable flow groups expose masks, phase and HDR color")
        self.material["naraka_dynamic_material"] = "periodic2"
        return self.multiply_color(combined, factor, "流光背面强度")

    def first(self, names):
        return next((name for name in names if self.textures.get(name, {}).get("texture")), None)

    def sample(self, prop, data=False, scroll=None, uv_override=None):
        if not prop or not self.textures.get(prop, {}).get("texture"):
            return None
        if (prop, data) in self.sampled and scroll is None and uv_override is None:
            return self.sampled[prop, data]
        binding = self.textures[prop]
        self.used.add(prop)
        uv = self.node("ShaderNodeUVMap", prop + " UV", -1300, -len(self.sampled) * 260)
        uv.uv_map = "UV0"
        if prop.endswith("UV2"):
            uv.uv_map = "UV1"
        # Per-layer UV selection is retained in the source. Ordinary cloth uses UV0.
        selection = self.vectors.get("_TextureUVSelection", [0, 0, 0, 0])
        if prop == "_MainTexture" and selection[0] != 0:
            uv.uv_map = "UV1"
        mapping = self.node("ShaderNodeMapping", prop + " scale / offset", -1100, uv.location.y)
        mapping.vector_type = "POINT"
        self.links.new(uv.outputs[0], mapping.inputs[0])
        scale = binding.get("scale", [1, 1])
        offset = binding.get("offset", [0, 0])
        # Uber effect shaders use explicit vectors in addition to TextureEnv ST.
        stem = {"_MainTexture": "_MainTexture", "_SecondTexture": "_SecondTexture",
            "_MaskTexture": "_MaskTexture", "_DistortTexture": "_Distort"}.get(prop)
        if stem and stem + "ScaleOffset" in self.vectors:
            scale_offset = self.vectors[stem + "ScaleOffset"]
            scale = [scale[i] * scale_offset[i] for i in range(2)]
            offset = [offset[i] + scale_offset[i + 2] for i in range(2)]
        mapping.inputs["Scale"].default_value = (*scale, 1)
        mapping.inputs["Location"].default_value = (*offset, 0)
        movement = scroll or (self.vectors.get(stem + "ScrollRotate") if stem else None)
        if movement is None and stem:
            movement = self.vectors.get(stem + "ScrollAnchor")
        if movement:
            for axis in range(2):
                if movement[axis] != 0:
                    driver = mapping.inputs["Location"].driver_add("default_value", axis).driver
                    for name, data_path in (("fps", "render.fps"), ("base", "render.fps_base")):
                        var = driver.variables.new(); var.name = name; var.type = "SINGLE_PROP"
                        var.targets[0].id_type = "SCENE"; var.targets[0].id = bpy.context.scene; var.targets[0].data_path = data_path
                    driver.expression = "%r+frame*base/fps*(%r)" % (offset[axis], movement[axis])
            if stem and stem + "ScrollRotate" in self.vectors:
                mapping.inputs["Rotation"].default_value[2] = math.radians(movement[2])
        node = self.node("ShaderNodeTexImage", prop, -850, uv.location.y)
        node["naraka_binding"] = prop
        node.image = self.builder.image(binding["texture"], data)
        source_texture = self.builder.manifest["textures"][binding["texture"]]
        node.extension = "EXTEND" if source_texture["wrap"] == 1 else "REPEAT"
        node.interpolation = "Closest" if source_texture["filter"] == 0 else "Linear"
        self.links.new(mapping.outputs[0], node.inputs[0])
        if uv_override is not None:
            self.links.new(uv_override, node.inputs[0])
        self.sampled[prop, data] = node
        return node

    def separate(self, node):
        result = self.node("ShaderNodeSeparateColor", node.label + " channels", -550, node.location.y)
        if node.get("naraka_binding"):
            result["naraka_binding"] = node["naraka_binding"]
            if "naraka_array_slice" in node:
                result["naraka_array_slice"] = node["naraka_array_slice"]
        self.links.new(node.outputs["Color"], result.inputs[0])
        return result.outputs

    def normal(self, prop):
        texture = self.sample(prop, True)
        channels = self.separate(texture)
        storage = self.builder.manifest["textures"][self.textures[prop]["texture"]].get("storageFormat", "")
        if "BC5" in storage or (prop == "_NormalBentMap" and self.source["shader"].lower().endswith("lx22standardcharacterskin.shader")):
            x = self.math("MULTIPLY", channels[0], 2)
            x = self.math("SUBTRACT", x, 1)
            y = self.math("SUBTRACT", self.math("MULTIPLY", channels[1], 2), 1)
            length = self.math("ADD", self.math("MULTIPLY", x, x), self.math("MULTIPLY", y, y))
            z = self.math("SQRT", self.math("MAXIMUM", self.math("SUBTRACT", 1, length), 0))
            rgb = self.node("ShaderNodeCombineColor", "BC5 reconstructed normal", -100, -300)
            self.links.new(channels[0], rgb.inputs[0]); self.links.new(channels[1], rgb.inputs[1])
            self.links.new(self.math("ADD", self.math("MULTIPLY", z, 0.5), 0.5), rgb.inputs[2])
            color = rgb.outputs[0]
        elif storage in ("DXT5", "BC3"):
            # Unity's DXT5nm stores X in alpha and Y in green.
            x = self.math("SUBTRACT", self.math("MULTIPLY", self.math("MULTIPLY", texture.outputs["Alpha"], channels[0]), 2), 1)
            y = self.math("SUBTRACT", self.math("MULTIPLY", channels[1], 2), 1)
            z = self.math("SQRT", self.math("MAXIMUM", self.math("SUBTRACT", 1,
                self.math("ADD", self.math("MULTIPLY", x, x), self.math("MULTIPLY", y, y))), 0))
            rgb = self.node("ShaderNodeCombineColor", "Unity DXT5nm", -100, -300)
            self.links.new(self.math("MULTIPLY", texture.outputs["Alpha"], channels[0]), rgb.inputs[0]); self.links.new(channels[1], rgb.inputs[1])
            self.links.new(self.math("ADD", self.math("MULTIPLY", z, 0.5), 0.5), rgb.inputs[2])
            color = rgb.outputs[0]
        else:
            color = texture.outputs["Color"]
        normal = self.node("ShaderNodeNormalMap", "Tangent normal", 100, -200)
        normal.inputs["Strength"].default_value = self.floats.get("_BumpScale", 1)
        normal.uv_map = "UV0"
        self.links.new(color, normal.inputs["Color"])
        return normal.outputs[0]

    def build(self):
        shader = self.source["shader"].lower()
        cloth = "cloth" in shader
        skin = "characterskin" in shader
        effect = any(t in shader for t in ("uber", "particle", "unlit")) and not cloth
        self.builder.adapters[self.source["id"]] = "effect-preview" if effect else "cloth-pbr-preview" if cloth else "skin-pbr-preview" if skin else "generic-pbr-preview"
        self.section = "output"
        output = self.node("ShaderNodeOutputMaterial", "Material output", 900, 0)
        bsdf = self.node("ShaderNodeBsdfPrincipled", "Naraka reconstructed surface", 600, 0)
        self.links.new(bsdf.outputs[0], output.inputs["Surface"])
        self.section = "base"
        base_prop = self.first(("_MainTex", "_BaseMap", "_MainTexture", "_DiffuseTexture", "_BaseColorMap"))
        base = self.sample(base_prop)
        tint = self.vectors.get("_Color", self.vectors.get("_BaseColor", [1, 1, 1, 1]))
        if base:
            self.links.new(self.multiply_color(base.outputs["Color"], tint, "Base color × source tint"), bsdf.inputs["Base Color"])
        else:
            bsdf.inputs["Base Color"].default_value = tint
            self.notes.append("No recognized base color binding")
        roughness = self.floats.get("_Roughness", 1 - self.floats.get("_Glossiness", 0.55))
        bsdf.inputs["Roughness"].default_value = max(0, min(1, roughness))
        bsdf.inputs["Metallic"].default_value = self.floats.get("_Metallic", 0)
        if skin:
            bsdf.inputs["Subsurface Weight"].default_value = 0.08
            self.notes.append("Skin uses approximate Principled subsurface; the game's skin/bent-normal shader is not executed")
        self.section = "normal"
        normal_prop = self.first(("_BumpMap", "_NormalMap", "_NormalTexture", "_NormalBentMap"))
        normal = None
        if normal_prop:
            normal = self.normal(normal_prop)
            self.links.new(normal, bsdf.inputs["Normal"])
        self.section = "surface"
        packed_prop = self.first(("_SpecGlossMap", "_MetallicGlossMap", "_MaskMap"))
        if packed_prop:
            packed = self.sample(packed_prop, True)
            tex_name = self.builder.manifest["textures"][self.textures[packed_prop]["texture"]]["name"].lower()
            channels = self.separate(packed)
            if shader.endswith("lx22standardcharacterskin.shader"):
                self.links.new(self.math("SUBTRACT", 1, self.math("POWER", self.saturate(channels[0]), self.floats.get("_GlossMapScale", 1))), bsdf.inputs["Roughness"])
                bsdf.inputs["Metallic"].default_value = self.vectors.get("_SkinColor", [0,0,0,0])[3] * self.floats.get("_SkinMetallic", 0)
                self.notes.append("Skin SpecGloss R is smoothness, G AO and B thickness; AO/thickness feed LX22 lighting/SSS, not cloth MRAV metallic")
                self.verified.append("Skin packed-map R smoothness and RG tangent normal")
            elif "_mrav" in tex_name:
                self.material["naraka_mrav_metallic_channel"] = 0
                self.material["naraka_mrav_roughness_channel"] = 1
                self.links.new(channels[0], bsdf.inputs["Metallic"])
                self.links.new(self.math("ADD", 1, self.math("MULTIPLY", self.math("SUBTRACT", channels[1], 1),
                    self.floats.get("_GlossMapScale", 1))), bsdf.inputs["Roughness"])
                self.notes.append("MRAV metallic R / scaled roughness G is verified for Periodic2; other shaders still need matching. AO B is retained as data because Blender handles ambient occlusion separately")
            elif packed_prop == "_MetallicGlossMap":
                self.links.new(channels[0], bsdf.inputs["Metallic"])
                self.links.new(self.math("SUBTRACT", 1, packed.outputs["Alpha"]), bsdf.inputs["Roughness"])
            else:
                self.notes.append("Packed specular channels kept as editable image nodes; channel semantics need shader-specific verification")
        self.section = "cloth"
        feature_prop = self.first(("_ClothFeatureTexture",))
        if feature_prop:
            feature = self.sample(feature_prop, True)
            masks = [*self.separate(feature), feature.outputs["Alpha"]]
            total = self.math("MAXIMUM", 1, self.math("ADD", self.math("ADD", masks[0], masks[1]), self.math("ADD", masks[2], masks[3])))
            masks = [self.math("DIVIDE", mask, total) for mask in masks]
            tint_sum = None
            for i, suffix in enumerate("RGBA"):
                color = self.vectors.get("_SpecularColor" + suffix, [1, 1, 1, 0])
                term = self.multiply_color(masks[i], color, "Specular region " + suffix)
                if tint_sum is None:
                    tint_sum = term
                else:
                    add = self.node("ShaderNodeMixRGB", "Combine specular regions", 200, 300)
                    add.blend_type = "ADD"; add.inputs[0].default_value = 1
                    self.links.new(tint_sum, add.inputs[1]); self.links.new(term, add.inputs[2]); tint_sum = add.outputs[0]
            self.links.new(tint_sum, bsdf.inputs["Specular Tint"])
            bsdf.inputs["Sheen Weight"].default_value = 0.15
            self.notes.append("Cloth regional specular/sheen is a Principled approximation; original anisotropy, iridescence and SSS parameters are preserved")
        self.section = "emission"
        from .advanced_cloth import AdvancedCloth, VERIFIED_SHADERS
        advanced = shader.rsplit("/", 1)[-1] in VERIFIED_SHADERS
        extra_emission, periodic_mask = None, 1
        if advanced:
            normal, extra_emission, periodic_mask, emission_uv = AdvancedCloth(self).build(bsdf)
        elif "lhmstandardcharacterclothadvanced_" in shader:
            self.notes.append("Unverified compiled AdvancedCloth variant; generic surface adapter, packed optical effects need variant inspection")
        emission_prop = self.first(("_EmissionMap", "_EmissionTexture", "_EmissiveMap"))
        if emission_prop:
            linear = self.builder.manifest["textures"][self.textures[emission_prop]["texture"]]["linear"]
            packed_periodic = ("periodic2" in shader or advanced) and (
                self.floats.get("_MFX_EMISSIVE_PERIODIC_2", 0) or
                self.keywords & {"_MFX_EMISSIVE_PERIODIC_2", "_MFX_EMISSIVE_PERIODIC_2_ON"})
            texture = self.sample(emission_prop, True if packed_periodic else linear, uv_override=emission_uv if advanced else None)
            emission_color = self.vectors.get("_EmissionColor", [1, 1, 1, 1])
            if packed_periodic:
                emission = self.periodic_emission(texture, normal, advanced)
                if advanced:
                    emission = self.multiply_color(emission, periodic_mask, "Detail-controlled periodic emission")
            else:
                emission = self.multiply_color(texture.outputs["Color"], emission_color, "Source HDR emission")
            self.links.new(emission, bsdf.inputs["Emission Color"])
            bsdf.inputs["Emission Strength"].default_value = 1
        if extra_emission is not None:
            self.section = "emission_mix"
            combined = self.add_color(emission, extra_emission, "Cloth effects + periodic emission") if emission_prop else extra_emission
            self.links.new(combined, bsdf.inputs["Emission Color"])
            bsdf.inputs["Emission Strength"].default_value = 1
        uber = shader.endswith("fx_uber_transparent.shadergraph")
        effect_result = None
        if uber and base:
            from .uber import build_uber
            effect_result = build_uber(self)
        elif effect and base:
            self.links.new(base.outputs["Color"], bsdf.inputs["Emission Color"])
            bsdf.inputs["Emission Strength"].default_value = self.floats.get("_EmissionIntensity", 1)
            bsdf.inputs["Base Color"].default_value = (0, 0, 0, 1)
        # Match face culling with Geometry Backfacing, including front-face cull
        # materials paired with a separate back-face renderer in Naraka cloth.
        self.section = "alpha"
        opacity = resolve_alpha(self.source)
        cull = opacity["cull"]
        geometry = self.node("ShaderNodeNewGeometry", "Face culling", 0, -650)
        visibility = geometry.outputs["Backfacing"] if cull == 1 else self.math("SUBTRACT", 1, geometry.outputs["Backfacing"]) if cull == 2 else 1
        uses_alpha = opacity["useBaseAlpha"]
        source_alpha = self.math("MULTIPLY", base.outputs["Alpha"], tint[3], "Material enabled diffuse alpha") if base and uses_alpha else tint[3] if uses_alpha else 1
        alpha = source_alpha
        if opacity["silkCoverage"]:
            # Verified DoubleTransparentPass (Periodic2 776 / legacy silk 1216):
            # u=saturate(tintedA/max(.01,threshold)); opacity is view dependent.
            coverage = self.saturate(self.math("DIVIDE", source_alpha, max(0.01, opacity["cutoff"])))
            dot = self.node("ShaderNodeVectorMath", "Silk normal · view")
            dot.operation = "DOT_PRODUCT"
            self.links.new(normal or geometry.outputs["Normal"], dot.inputs[0])
            self.links.new(geometry.outputs["Incoming"], dot.inputs[1])
            facing = self.math("MAXIMUM", self.math("ABSOLUTE", dot.outputs["Value"]), 2**-24)
            power = self.math("POWER", self.math("SUBTRACT", 1, coverage), self.math("DIVIDE", 1, facing))
            alpha = self.math("MULTIPLY", self.math("SUBTRACT", 1, power),
                self.math("MINIMUM", 1, self.math("MULTIPLY", coverage, 255)), "Silk transparent-pass coverage")
            self.notes.append("Silk alpha equation verified for the inspected DoubleTransparentPass variants; Blender approximates multi-pass composition")
        if opacity["cutout"] and uses_alpha:
            mask = self.math("SUBTRACT", 1, self.math("LESS_THAN", source_alpha, opacity["cutoff"]), "Material alpha test")
            alpha = self.math("MULTIPLY", alpha, mask) if opacity["blend"] else mask
        if not opacity["resolved"]:
            self.notes.append("Shader alpha usage unresolved; diffuse A is retained as data without opacity wiring")
        if opacity["depthOnly"]:
            alpha = 0
            self.notes.append("Dedicated depth-pass surface hidden in Blender; the game's separate depth-buffer contribution is not reproduced")
        self.links.new(self.math("MULTIPLY", alpha, visibility), bsdf.inputs["Alpha"])
        if effect_result is not None:
            self.section = "output"
            rgb, effect_alpha = effect_result
            emission_node = self.node("ShaderNodeEmission", "Uber emission surface")
            transparent = self.node("ShaderNodeBsdfTransparent", "Uber background")
            additive = self.floats.get("_SrcBlend") == 1 and self.floats.get("_DstBlend") == 1
            if additive:
                self.assign(emission_node.inputs["Color"], self.multiply_color(rgb, visibility, "Effect face culling"))
                compose = self.node("ShaderNodeAddShader", "Additive One / One effect")
                self.links.new(transparent.outputs[0], compose.inputs[0]); self.links.new(emission_node.outputs[0], compose.inputs[1])
            else:
                # Straight-alpha equivalent of premultiplied source output.
                self.assign(emission_node.inputs["Color"], self.multiply_color(rgb, self.math("DIVIDE", 1, self.math("MAXIMUM", effect_alpha, .00001)), "Unpremultiply for Blender closure"))
                compose = self.node("ShaderNodeMixShader", "Effect alpha composition")
                self.assign(compose.inputs[0], self.math("MULTIPLY", effect_alpha, visibility))
                self.links.new(transparent.outputs[0], compose.inputs[1]); self.links.new(emission_node.outputs[0], compose.inputs[2])
            self.links.new(compose.outputs[0], output.inputs["Surface"])
        self.material.use_backface_culling = cull == 2
        if hasattr(self.material, "surface_render_method"):
            self.material.surface_render_method = "DITHERED"
        elif hasattr(self.material, "blend_method"):
            self.material.blend_method = "HASHED"
        self.section = "retained"
        retained_bindings = []
        for name, binding in self.textures.items():
            if binding.get("texture") and name not in self.used:
                self.sample(name, True)
                retained_bindings.append(name)
                self.notes.append("Binding retained without automatic shader wiring: " + name)
        handled = {"_UV_DISTORT", "_TEXTURE_ROTATION", "_MASK_LAYER", "_SECOND_LAYER"} if uber else set()
        active_fx = [name for name, value in self.floats.items() if value and name not in handled and
            (name.startswith("_MFX_") or name in ("_UV_DISTORT", "_FRESNEL", "_EDGE_DISSOLVE")) and
            not (name == "_MFX_EMISSIVE_PERIODIC_2" and self.material.get("naraka_dynamic_material") == "periodic2")]
        if active_fx:
            self.notes.append("Shader-specific features need visual matching: " + ", ".join(active_fx))
        status = {
            "surface": "approximate",
            "verifiedFeatures": self.verified + (["Periodic2 RG/BA waves and source HDR tints"] if self.material.get("naraka_dynamic_material") == "periodic2" else []),
            "alpha": opacity,
            "retainedTextureBindings": retained_bindings,
            "pendingShaderFeatures": active_fx,
            "notes": self.notes,
        }
        self.material["naraka_conversion_status_json"] = json.dumps(status, ensure_ascii=False)
        self.builder.statuses[self.source["id"]] = status
        self.material["naraka_conversion_notes"] = json.dumps(self.notes, ensure_ascii=False)
        self.builder.report.extend(self.source["name"] + ": " + note for note in self.notes)
        # A specialized adapter replaces generic surface branches. Remove only
        # unreachable branches; intentionally retained source-image nodes stay.
        keep = set()
        stack = [output] + [n for n in self.nodes if n.get("naraka_section") == "retained"]
        while stack:
            n = stack.pop()
            if n in keep:
                continue
            keep.add(n)
            stack.extend(link.from_node for socket in n.inputs for link in socket.links)
        for n in list(self.nodes):
            if n not in keep:
                self.nodes.remove(n)
        if self.builder.readable:
            from .readable import organize
            organize(self.material)
        else:
            arrange_material(self.material)
        return self.material
