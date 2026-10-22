"""Installed AdvancedCloth surface equations, adapted to Blender's BRDF.

Evidence: ForwardLit D3D11 blob 4 / vertex blob 0 in e0de75c0,
d47dec16, ea693cd0, bab3fa28, ce653701, ca4bed42. See shader-contracts.md.
Lighting, thin-film interference and the game's multi-pass compositor are
not reproduced by Principled. Mask channels are never used as opacity.
"""
import math
import colorsys

VERIFIED_SHADERS = {
    "lhmstandardcharacterclothadvanced_" + suffix + ".shader"
    for suffix in ("e0de75c0", "d47dec16", "ea693cd0", "bab3fa28", "ce653701", "ca4bed42")
}


class AdvancedCloth:
    def __init__(self, graph):
        self.g = graph
        self.v, self.f, self.k = graph.vectors, graph.floats, graph.keywords
        self.M = graph.math

    def vec(self, xyz, label="Vector"):
        n = self.g.node("ShaderNodeCombineXYZ", label)
        for s, v in zip(n.inputs, xyz):
            self.g.assign(s, v)
        return n.outputs[0]

    def split(self, value):
        n = self.g.node("ShaderNodeSeparateXYZ", "Vector components")
        self.g.assign(n.inputs[0], value)
        return n.outputs

    def V(self, operation, a, b=(0, 0, 0), scale=1, label=""):
        n = self.g.node("ShaderNodeVectorMath", label or operation)
        n.operation = operation
        self.g.assign(n.inputs[0], a); self.g.assign(n.inputs[1], b)
        if operation == "SCALE":
            self.g.assign(n.inputs[3], scale)
        return n.outputs[1 if operation in ("DOT_PRODUCT", "LENGTH", "DISTANCE") else 0]

    def lerp(self, a, b, t, label="Mix scalar"):
        return self.M("ADD", a, self.M("MULTIPLY", t, self.M("SUBTRACT", b, a)), label)

    def uv(self, index=0):
        n = self.g.node("ShaderNodeUVMap", "Source UV%d" % index)
        n.uv_map = "UV%d" % index
        return n.outputs[0]

    def rotate_uv(self, uv, degrees):
        xy = self.split(uv)
        x, y = self.M("SUBTRACT", xy[0], .5), self.M("SUBTRACT", xy[1], .5)
        c, s = math.cos(math.radians(degrees)), math.sin(math.radians(degrees))
        return self.vec((self.M("ADD", .5, self.M("ADD", self.M("MULTIPLY", x, c), self.M("MULTIPLY", y, s))),
                         self.M("ADD", .5, self.M("SUBTRACT", self.M("MULTIPLY", y, c), self.M("MULTIPLY", x, s))), 0), "Detail rotation about UV centre")

    def transform_uv(self, uv, st):
        return self.V("ADD", self.V("MULTIPLY", uv, (*st[:2], 1)), (*st[2:], 0))

    def array(self, index, uv, label):
        binding = self.g.textures.get("_DetailTillingArray", {})
        identity = binding.get("texture")
        source = self.g.builder.manifest["textures"].get(identity, {})
        if not source.get("slices"):
            return None
        n = self.g.node("ShaderNodeTexImage", label)
        n.image = self.g.builder.image(identity, True, index)
        n.extension = "EXTEND" if source["wrap"] == 1 else "REPEAT"
        n.interpolation = "Closest" if source["filter"] == 0 else "Linear"
        n["naraka_binding"] = "_DetailTillingArray"
        n["naraka_array_slice"] = index
        self.g.assign(n.inputs[0], uv)
        self.g.used.add("_DetailTillingArray")
        return n

    def unpack(self, texture, strength=1, alpha_x=True):
        cs = self.g.separate(texture)
        # The converter supplies BC5's missing A as 1. DXT5nm uses R*A.
        red = self.M("MULTIPLY", cs[0], texture.outputs["Alpha"]) if alpha_x else cs[0]
        x = self.M("MULTIPLY", self.M("SUBTRACT", self.M("MULTIPLY", red, 2), 1), strength)
        y = self.M("MULTIPLY", self.M("SUBTRACT", self.M("MULTIPLY", cs[1], 2), 1), strength)
        z = self.M("SQRT", self.M("MAXIMUM", self.M("SUBTRACT", 1, self.M("ADD", self.M("MULTIPLY", x, x), self.M("MULTIPLY", y, y))), 0))
        return self.vec((x, y, z), "Unpacked tangent normal"), cs

    def world_normal(self, tangent):
        color = self.V("ADD", self.V("SCALE", tangent, scale=.5), (.5, .5, .5))
        node = self.g.node("ShaderNodeNormalMap", "Advanced cloth tangent normal")
        node.uv_map = "UV0"
        self.g.assign(node.inputs["Color"], color)
        return node.outputs[0]

    def merge_normal(self, base, detail):
        a, b = self.split(base), self.split(detail)
        return self.V("NORMALIZE", self.vec((self.M("ADD", a[0], b[0]), self.M("ADD", a[1], b[1]), self.M("MULTIPLY", a[2], b[2]))))

    def view(self, normal):
        return self.V("DOT_PRODUCT", normal, self.geo.outputs["Incoming"])

    def build(self, bsdf):
        g = self.g
        g.section = "detail"
        self.geo = g.node("ShaderNodeNewGeometry", "Cloth view and geometric frame")
        uv0 = self.uv()
        main_binding = g.textures.get("_MainTex", {})
        uv0 = self.transform_uv(uv0, [*main_binding.get("scale", [1, 1]), *main_binding.get("offset", [0, 0])])
        main = g.sample("_MainTex", False, uv_override=uv0)
        color = main.outputs["Color"]
        bump = g.sample("_BumpMap", True, uv_override=uv0)
        base_n, _ = self.unpack(bump, self.f.get("_BumpScale", 1))
        normal = base_n
        mr = g.sample("_SpecGlossMap", True, uv_override=uv0)
        mr_cs = g.separate(mr)
        metal, rough = mr_cs[0], mr_cs[1]
        spec = self.v.get("_SpecularTint", [1, 1, 1, 1])
        tiling = g.sample("_TillingChannelMask", True, uv_override=uv0)
        masks = [*g.separate(tiling)[:3], tiling.outputs["Alpha"]] if tiling else [0]*4
        if "_USEUV2" in self.k:
            uv2_bump = g.sample("_BumpMapUV2", True, uv_override=self.uv(1))
            if uv2_bump:
                select = self.v.get("_UseUV2DetailMask", [0]*4)
                masks = [g.saturate(self.M("ADD",m,self.M("MULTIPLY",uv2_bump.outputs["Alpha"],s))) for m,s in zip(masks,select)]
        detail_uvs = [self.transform_uv(self.rotate_uv(uv0, self.v.get("_DetailMap%dProp2" % i, [0]*4)[3]),
            self.v.get("_DetailTilling_ST%d" % i, [1, 1, 0, 0])) for i in range(1, 5)]
        details = {}
        weights = {}
        for i in (3, 4):
            if "_UseDetail%d" % i not in self.k:
                continue
            tex = self.array(i-1, detail_uvs[i-1], "Detail layer %d: RG normal / B mask" % i)
            if tex is None:
                g.notes.append("Active detail array missing: re-export with AnimeStudio's array-capable exporter")
                continue
            prop1 = self.v.get("_DetailMap%dProp1" % i, [0, 0, .5, .5])
            prop2 = self.v.get("_DetailMap%dProp2" % i, [0, 0, 1, 0])
            detail_n, channels = self.unpack(tex, prop2[2], False)
            mask = masks[i-1]
            weight = self.M("MULTIPLY", mask, channels[2], "Detail %d weighted mask" % i)
            details[i], weights[i] = channels[2], weight
            normal = g.mix(normal, self.merge_normal(normal, detail_n), mask, "Detail %d blended normal" % i)
            tint = self.v.get("_DetailMap%dColor" % i, [0, 0, 0, 0])
            color = g.mix(color, tint, self.M("MULTIPLY", weight, tint[3]), "Detail %d base tint" % i)
            tint = self.v.get("_DetailMap%dSpecColor" % i, [1, 1, 1, 0])
            spec = g.mix(spec, tint, self.M("MULTIPLY", weight, tint[3]), "Detail %d specular tint" % i)
            metal, rough = self.lerp(metal, prop1[2], weight), self.lerp(rough, prop1[3], weight)
        flags = int(self.f.get("_EffectByDetailMap", 255))
        def detail_gate(i):
            mask = masks[i-1]
            b = details.get(i, 0)
            return self.M("ADD", self.M("MULTIPLY", self.M("MULTIPLY", b, mask), flags & (1 << (2*(i-1)))),
                self.M("MULTIPLY", self.M("MULTIPLY", self.M("SUBTRACT", 1, b), mask), flags & (2 << (2*(i-1)))))
        gate3 = g.saturate(detail_gate(3)) if 3 in details else 0
        gate = self.lerp(1, gate3, masks[2]) if 3 in details else 1
        if 4 in details:
            d4 = detail_gate(4)
            gate = g.saturate(self.lerp(self.M("ADD", gate, d4), g.saturate(self.M("ADD", gate3, d4)), masks[3]))
        overlay_alpha = None
        if "_USEUV2" in self.k:
            g.section = "uv2"
            uv1 = self.uv(1)
            overlay = g.sample("_MainTexUV2", False, uv_override=uv1)
            overlay_mr = g.sample("_SpecGlossMapUV2", True, uv_override=uv1)
            overlay_bump = g.sample("_BumpMapUV2", True, uv_override=uv1)
            if overlay and overlay_mr and overlay_bump:
                overlay_alpha = overlay.outputs["Alpha"]
                color = g.mix(color, overlay.outputs["Color"], overlay_alpha, "UV2 overlay: diffuse A is layer weight")
                cs = g.separate(overlay_mr)
                metal, rough = self.lerp(metal, cs[0], overlay_alpha), self.lerp(rough, cs[1], overlay_alpha)
                overlay_n, _ = self.unpack(overlay_bump, alpha_x=False)
                normal = g.mix(normal, self.merge_normal(base_n, overlay_n), overlay_alpha, "UV2 normal overlay")
                g.verified.append("UV2 diffuse-A overlay, MRAV and normal blending")
        g.section = "cloth"
        identity = g.sample("_AdvIDTex", True, uv_override=uv0)
        ids = [*g.separate(identity)[:3], identity.outputs["Alpha"]] if identity else [0]*4
        if overlay_alpha is not None:
            extra = self.f.get("_UseExtraEffectInUv1", 0)
            ids = [self.M("MULTIPLY", x, self.lerp(1, extra, overlay_alpha)) for x in ids]
        features = self.v.get("_ShaderFeatureParam1", [0]*4)
        velvet = self.M("MULTIPLY", ids[0], self.f.get("_VELVET_Str", 0)) if features[3] else 0
        aniso = self.M("MULTIPLY", ids[1], self.f.get("_AnisoAspect", 0)) if features[1] else 0
        irid = self.M("MULTIPLY", ids[3], self.f.get("_IridescenceStr", 0)) if features[0] else 0
        distance = self.f.get("_Distance", 0)
        for i, weight in weights.items():
            p1, p2 = self.v["_DetailMap%dProp1" % i], self.v["_DetailMap%dProp2" % i]
            aniso = self.lerp(aniso, p1[0], weight) if features[1] else 0
            irid = self.lerp(irid, p1[1], weight) if features[0] else 0
            distance = self.lerp(distance, p2[1], weight)
        g.assign(bsdf.inputs["Sheen Weight"], g.saturate(velvet))
        # Principled has nonnegative anisotropy; a quarter turn accounts for
        # the sign of the source aspect. Its lobe still differs from LX22.
        g.assign(bsdf.inputs["Anisotropic"], g.saturate(self.M("ABSOLUTE", aniso)))
        g.assign(bsdf.inputs["Anisotropic Rotation"], self.M("MULTIPLY", self.M("LESS_THAN", aniso, 0), .25))
        g.assign(bsdf.inputs["Specular Tint"], g.mix((1, 1, 1, 1), spec, ids[1] if features[1] else 0, "ID G weighted cloth specular"))
        if features[2]:
            through = self.M("MULTIPLY", ids[2], self.f.get("_Scale", 0))
            g.assign(bsdf.inputs["Subsurface Weight"], g.saturate(through))
            g.assign(bsdf.inputs["Subsurface Radius"], self.v.get("_PassThoughColor", [1,1,1,1])[:3])
        if features[0]:
            g.notes.append("ID A iridescence and detail thickness retained; LX22 interference BRDF requires a custom shading model")
        g.verified.append("AdvID RGBA: velvet / anisotropy / transmission / iridescence; tiling-mask detail layers 3 and 4")
        g.notes.append("ID-driven velvet, anisotropy and transmission use Principled lobes; game BRDF, AO lighting and iridescence are approximations")
        g.section = "optical"
        extra_emission = None
        for i in (1, 2):
            if not self.k & {"_MASKED_DIAMONDS%d" % i, "_MASKED_GLITTER%d" % i}:
                continue
            tex = self.array(i-1, detail_uvs[i-1], "Optical layer %d packed data" % i)
            if tex is None:
                g.notes.append("Active diamond/glitter layer %d needs exported array pixels" % i)
                continue
            mask = self.M("MULTIPLY", masks[i-1], gate)
            if "_MASKED_GLITTER%d" % i in self.k:
                color, normal, metal, rough, term = self.glitter(i, tex, mask, color, normal, metal, rough, base_n)
            else:
                color, normal, rough, term = self.diamond(i, tex, detail_uvs[i-1], mask, color, normal, rough, main.outputs["Color"])
            extra_emission = g.add_color(extra_emission, term, "Optical layers") if extra_emission is not None else term
        g.section = "surface"
        g.assign(bsdf.inputs["Base Color"], g.multiply_color(color, self.v.get("_Color", [1]*4), "Advanced source base tint"))
        g.assign(bsdf.inputs["Metallic"], metal)
        g.assign(bsdf.inputs["Roughness"], self.M("ADD", 1, self.M("MULTIPLY", self.M("SUBTRACT", rough, 1), self.f.get("_GlossMapScale", 1))))
        world_n = self.world_normal(normal)
        g.assign(bsdf.inputs["Normal"], world_n)
        selector = int(self.f.get("_EffectUvSelector", 1))
        effect_uv = self.vec((0, 0, 0))
        for index in range(3):
            if selector & (1 << index):
                effect_uv = self.V("ADD", effect_uv, uv0 if index == 0 else self.uv(index))
        transform = self.v.get("_EffectUVTransform", [0,0,1,1])
        emission_uv = self.V("DIVIDE", self.V("SUBTRACT", effect_uv, (*transform[:2],0)), (*transform[2:],1))
        return world_n, extra_emission, gate, emission_uv

    def diamond(self, i, tex, uv, mask, color, normal, rough, source_color):
        g = self.g
        g.section = "diamond"
        p = self.v["_EffectUvParam%d" % i]
        n = self.v["_EffectNormalParam%d" % i]
        mr = self.v["_EffectMRParam%d" % i]
        cs = g.separate(tex)
        tangent = g.node("ShaderNodeTangent", "Diamond tangent frame")
        tangent.direction_type, tangent.uv_map = "UV_MAP", "UV0"
        bitangent = self.V("CROSS_PRODUCT", self.geo.outputs["Normal"], tangent.outputs[0])
        view = self.vec(tuple(self.V("DOT_PRODUCT", self.geo.outputs["Incoming"], axis) for axis in (tangent.outputs[0],bitangent,self.geo.outputs["Normal"])))
        optical_n = self.V("NORMALIZE", self.vec((self.M("MULTIPLY", self.M("SUBTRACT", self.M("MULTIPLY", cs[0],2),1), p[2]), self.M("MULTIPLY", self.M("SUBTRACT", self.M("MULTIPLY", cs[1],2),1),p[2]),1)))
        # refract(-view, opticalNormal, eta), as in the compiled variant.
        incoming = self.V("SCALE", view, scale=-1)
        dot = self.V("DOT_PRODUCT", optical_n, incoming)
        k = self.M("SUBTRACT", 1, self.M("MULTIPLY", p[0]**2, self.M("SUBTRACT",1,self.M("MULTIPLY",dot,dot))))
        refracted = self.V("SUBTRACT", self.V("SCALE",incoming,scale=p[0]), self.V("SCALE",optical_n,scale=self.M("ADD",self.M("MULTIPLY",p[0],dot),self.M("SQRT",self.M("MAXIMUM",k,0)))))
        ref = self.split(refracted)
        depth = self.M("MULTIPLY",p[3],self.M("ADD",cs[2],1))
        shifted = self.V("SUBTRACT",uv,self.vec((self.M("MULTIPLY",self.M("DIVIDE",ref[0],ref[2]),depth),self.M("MULTIPLY",self.M("DIVIDE",ref[1],ref[2]),depth),0)))
        refracted_tex = self.array(i-1,shifted,"Diamond %d refracted A" % i)
        a = self.M("MULTIPLY",refracted_tex.outputs["Alpha"],self.M("GREATER_THAN",k,0))
        spectrum = []
        for phase in (0,2/3,1/3):
            wave = self.M("SUBTRACT", self.M("MULTIPLY",self.M("FRACT",self.M("ADD",self.M("MULTIPLY",a,4*p[1]),phase)),6),3)
            channel = g.saturate(self.M("SUBTRACT",self.M("ABSOLUTE",wave),1))
            spectrum.append(self.M("MULTIPLY",a,self.lerp(1,channel,p[1])))
        shade = g.saturate(self.M("MULTIPLY",self.M("SUBTRACT",a,.5),n[2]+1))
        tint = self.v["_BaseColor%d" % i]
        target = g.mix(g.multiply_color(source_color,tint,"Diamond source tint"),tint,tint[3],"Diamond tint mode")
        color = g.mix(color,g.multiply_color(target,shade,"Diamond refracted diffuse"),mask,"Masked diamond diffuse")
        rough = self.lerp(rough,mr[1],mask)
        raw = self.V("NORMALIZE",self.V("SUBTRACT",tex.outputs["Color"],(.5,.5,.5)))
        time = self.M("MULTIPLY",g.time.outputs[0],mr[0]/20)
        angle = self.M("MULTIPLY",self.M("FRACT",time),6.28)
        r = self.split(raw); c,s = self.M("COSINE",angle),self.M("SINE",angle)
        rotated = self.vec((self.M("ADD",self.M("MULTIPLY",r[0],c),self.M("MULTIPLY",r[2],s)),r[1],self.M("SUBTRACT",self.M("MULTIPLY",r[2],c),self.M("MULTIPLY",r[0],s))))
        normal = self.V("NORMALIZE",g.mix(normal,rotated,self.M("MULTIPLY",mask,n[3]),"Diamond animated normal"))
        glow = self.v["_GlitterEmissionColor%d" % i]
        glow_color = g.mix(g.multiply_color(source_color,glow,"Diamond source emission tint"),glow,glow[3],"Diamond emission tint mode")
        sparkle = g.saturate(self.M("ADD",.5,self.M("MULTIPLY",self.M("SUBTRACT",a,.5),n[1]+1)))
        term = g.multiply_color(glow_color,self.M("MULTIPLY",mask,sparkle),"Masked diamond emission")
        g.notes.append("Diamond refraction samples source array A; optical diffuse/normal/emission adapted, LX22 specular highlight uses Principled")
        return color, normal, rough, term

    def glitter(self, i, tex, mask, color, normal, metal, rough, base_n):
        g = self.g
        g.section = "glitter"
        p,n,mr = (self.v[name+str(i)] for name in ("_EffectUvParam","_EffectNormalParam","_EffectMRParam"))
        raw,cs = self.unpack(tex, alpha_x=False)
        factor = self.lerp(p[3],1,tex.outputs["Alpha"])
        tint = self.v["_BaseColor%d" % i]
        color = g.mix(color,tint,self.M("MULTIPLY",self.M("MULTIPLY",mask,factor),tint[3]),"Glitter base tint")
        strength = self.M("MULTIPLY",self.M("MULTIPLY",factor,p[1]),mask)
        metal,rough = self.lerp(metal,mr[0],strength),self.lerp(rough,mr[1],strength)
        time = self.M("MULTIPLY",g.time.outputs[0],.005)
        angle = self.M("MULTIPLY",self.M("FRACT",self.M("MULTIPLY",self.M("ADD",self.M("FRACT",time),cs[1]),n[3])),62.8)
        xyz = self.split(raw); s,c = self.M("SINE",angle),self.M("COSINE",angle)
        rotated = self.vec((self.M("ADD",self.M("MULTIPLY",xyz[0],c),self.M("MULTIPLY",xyz[1],s)),self.M("SUBTRACT",self.M("MULTIPLY",xyz[1],c),self.M("MULTIPLY",xyz[0],s)),xyz[2]))
        changed = self.V("NORMALIZE",self.V("ADD",base_n,self.V("SCALE",rotated,scale=self.M("MULTIPLY",factor,p[0]*n[0]))))
        normal = g.mix(normal,changed,mask,"Glitter animated normal")
        facing_n = self.V("NORMALIZE",g.mix(base_n,raw,n[0],"Glitter facing normal"))
        glow = self.v["_GlitterEmissionColor%d" % i]
        facing = self.M("POWER",self.M("ABSOLUTE",self.view(self.world_normal(facing_n))),glow[3])
        front = self.M("POWER",g.saturate(facing,.001,.999),max(0,16-p[2]*32))
        rim = self.M("POWER",g.saturate(self.M("SUBTRACT",1,facing),.001,.999),max(0,p[2]*32-16))
        phase = self.M("ADD",self.M("MULTIPLY",self.M("FRACT",self.M("MULTIPLY",self.M("FRACT",self.M("MULTIPLY",time,n[1])),10)),2*math.pi),self.M("MULTIPLY",cs[2],n[2]))
        wave = self.M("ADD",.5,self.M("MULTIPLY",.5,self.M("SINE",phase)))
        weight = self.M("MULTIPLY",self.M("MULTIPLY",mask,self.M("MINIMUM",front,rim)),wave)
        h,sat,value = colorsys.rgb_to_hsv(*tint[:3])
        n_dot_v = g.saturate(self.view(self.world_normal(base_n)))
        hue = self.M("ADD",h,self.M("MULTIPLY",n_dot_v,mr[3]))
        saturation = self.lerp(sat,1,self.M("MULTIPLY",n_dot_v,mr[3]))
        rgb = []
        for phase,channel in zip((0,2/3,1/3),glow):
            stripe = g.saturate(self.M("SUBTRACT",self.M("ABSOLUTE",self.M("SUBTRACT",self.M("MULTIPLY",self.M("FRACT",self.M("ADD",hue,phase)),6),3)),1))
            colored = self.M("MULTIPLY",self.lerp(1,stripe,saturation),value*channel)
            rgb.append(g.saturate(self.M("MULTIPLY",colored,self.M("MINIMUM",front,rim)),0,32))
        # The compiled shader clamps the view-colored HDR result before the
        # sine wave and tiling-mask multipliers, not afterwards.
        term = g.multiply_color(self.vec(rgb,"Glitter HSV tint and HDR clamp"),self.M("MULTIPLY",mask,wave),"Glitter source HDR emission")
        g.notes.append("Glitter array RG normal / B phase / A modulation and source HDR color wired; source specular lobe differs from Principled")
        return color,normal,metal,rough,term
