"""Small editable PBR graphs, with packed phase waves and ordinary UV motion.

No detail arrays, glitter, refraction, view-dependent lobes or shader emulation.
Images and channel splits stay visible. Only reusable normal/wave math is grouped.
"""
import json
import math

import bpy

from .alpha import resolve_alpha
from .materials import Graph
from .readable import interface, quiet_previews


def helper_tree(contract, name, inputs, outputs, build):
    tree = next((t for t in bpy.data.node_groups if t.get("naraka_group_contract") == contract), None)
    if tree:
        return tree
    tree = bpy.data.node_groups.new(name, "ShaderNodeTree")
    tree["naraka_group_contract"] = contract
    for label, kind, default in inputs:
        interface(tree, label, "INPUT", kind, default)
    for label, kind in outputs:
        interface(tree, label, "OUTPUT", kind)
    inp = tree.nodes.new("NodeGroupInput")
    out = tree.nodes.new("NodeGroupOutput")
    def op(operation, a, b=0):
        node = tree.nodes.new("ShaderNodeMath")
        node.operation = operation
        for socket, value in zip(node.inputs, (a, b)):
            if hasattr(value, "is_output"):
                tree.links.new(value, socket)
            else:
                socket.default_value = value
        return node.outputs[0]
    build(tree, inp, out, op)
    arrange_lightweight(type("Tree", (), {"node_tree": tree})())
    quiet_previews(tree)
    return tree


def normal_tree():
    def build(tree, inp, out, op):
        x = op("SUBTRACT", op("MULTIPLY", inp.outputs["X 编码"], 2), 1)
        y = op("SUBTRACT", op("MULTIPLY", inp.outputs["Y 编码"], 2), 1)
        z = op("SQRT", op("MAXIMUM", op("SUBTRACT", 1,
            op("ADD", op("MULTIPLY", x, x), op("MULTIPLY", y, y))), 0))
        combine = tree.nodes.new("ShaderNodeCombineColor")
        tree.links.new(inp.outputs["X 编码"], combine.inputs[0])
        tree.links.new(inp.outputs["Y 编码"], combine.inputs[1])
        tree.links.new(op("ADD", op("MULTIPLY", z, .5), .5), combine.inputs[2])
        tree.links.new(combine.outputs[0], out.inputs["法线颜色"])
    return helper_tree("light-normal-rg-v1", "永劫 · RG法线重建Z", [
        ("X 编码", "NodeSocketFloat", .5), ("Y 编码", "NodeSocketFloat", .5)],
        [("法线颜色", "NodeSocketColor")], build)


def phase_tree():
    def build(tree, inp, out, op):
        v = inp.outputs
        angle = op("ADD", op("MULTIPLY", v["相位"], v["相位频率"]),
            op("MULTIPLY", v["时间 秒"], v["速度 rad/s"]))
        wave = op("ADD", op("MULTIPLY", op("SINE", angle), .5), .5)
        remap = tree.nodes.new("ShaderNodeMapRange")
        remap.clamp = True
        tree.links.new(wave, remap.inputs["Value"])
        tree.links.new(v["阈值"], remap.inputs["From Min"])
        remap.inputs["From Max"].default_value = 1
        mask = op("MULTIPLY", v["区域蒙版"], v["区域蒙版"])
        moving = op("MULTIPLY", mask, op("POWER", remap.outputs[0], v["锐度"]))
        color = tree.nodes.new("ShaderNodeMixRGB")
        color.blend_type = "MULTIPLY"
        color.inputs[0].default_value = 1
        tree.links.new(v["颜色"], color.inputs[1])
        tree.links.new(op("MULTIPLY", moving, v["强度"]), color.inputs[2])
        for socket, name in [(color.outputs[0], "发光"), (moving, "移动亮区"),
                             (v["区域蒙版"], "区域蒙版"), (v["相位"], "相位")]:
            tree.links.new(socket, out.inputs[name])
    inputs = [(name, "NodeSocketFloat", default) for name, default in [
        ("区域蒙版", 1), ("相位", 0), ("时间 秒", 0), ("相位频率", 1),
        ("速度 rad/s", 0), ("阈值", 0), ("锐度", 1), ("强度", 1)]]
    inputs.append(("颜色", "NodeSocketColor", (1, 1, 1, 1)))
    return helper_tree("light-phase-wave-v1", "永劫 · 相位流光（轻量）", inputs,
        [("发光", "NodeSocketColor"), ("移动亮区", "NodeSocketFloat"),
         ("区域蒙版", "NodeSocketFloat"), ("相位", "NodeSocketFloat")], build)


def arrange_lightweight(material):
    """Visible DAG columns; no collapsing or duplicated computation stages."""
    from .layout import node_size
    tree = material.node_tree
    nodes = [n for n in tree.nodes if n.type != "FRAME"]
    for n in nodes:
        n.parent = None
    parents = {n: [] for n in nodes}
    for link in tree.links:
        if link.to_node in parents and link.from_node in parents:
            parents[link.to_node].append(link.from_node)
    ranks, visiting = {}, set()
    def rank(node):
        if node in ranks:
            return ranks[node]
        if node in visiting:
            return 0
        visiting.add(node)
        ranks[node] = max((rank(p) + 1 for p in parents[node] if p not in visiting), default=0)
        visiting.remove(node)
        return ranks[node]
    columns = {}
    for n in nodes:
        columns.setdefault(rank(n), []).append(n)
    for col, members in columns.items():
        y = 600
        for n in sorted(members, key=lambda n: (n.get("naraka_section", ""), n.label, n.name)):
            n.width = 300 if n.type in {"GROUP", "TEX_IMAGE", "BSDF_PRINCIPLED"} else 250
            n.location = (col * 410, y)
            n.select = False
            y -= node_size(n)[1] + 85
    return len(columns)


class LightweightGraph(Graph):
    def vector(self, operation, a, b=(0, 0, 0)):
        n = self.node("ShaderNodeVectorMath", operation)
        n.operation = operation
        self.assign(n.inputs[0], a)
        self.assign(n.inputs[1], b)
        return n.outputs[0]

    def uv(self, index):
        if not hasattr(self, "uvs"):
            self.uvs = {}
        if index not in self.uvs:
            node = self.node("ShaderNodeUVMap", "UV%d · 第%d套" % (index, index + 1))
            node.uv_map = "UV%d" % index
            self.uvs[index] = node.outputs[0]
        return self.uvs[index]

    def sample(self, prop, data=False, scroll=None, uv_override=None):
        if not prop or not self.textures.get(prop, {}).get("texture"):
            return None
        key = (prop, data)
        if key in self.sampled:
            return self.sampled[key]
        binding = self.textures[prop]
        uv = uv_override if uv_override is not None else self.uv(1 if prop.endswith("UV2") else 0)
        scale, offset = binding.get("scale", [1, 1]), binding.get("offset", [0, 0])
        if scale != [1, 1] or offset != [0, 0] or scroll:
            mapping = self.node("ShaderNodeMapping", prop + " · 缩放/偏移/滚动")
            self.assign(mapping.inputs[0], uv)
            mapping.inputs["Scale"].default_value = (*scale, 1)
            mapping.inputs["Location"].default_value = (*offset, 0)
            if scroll:
                for axis in range(2):
                    if not scroll[axis]:
                        continue
                    driver = mapping.inputs["Location"].driver_add("default_value", axis).driver
                    for name, path in (("fps", "render.fps"), ("base", "render.fps_base")):
                        var = driver.variables.new()
                        var.name, var.type = name, "SINGLE_PROP"
                        var.targets[0].id_type, var.targets[0].id = "SCENE", bpy.context.scene
                        var.targets[0].data_path = path
                    driver.expression = "%r+frame*base/fps*(%r)" % (offset[axis], scroll[axis])
            uv = mapping.outputs[0]
        node = self.node("ShaderNodeTexImage", prop)
        node["naraka_binding"] = prop
        node.image = self.builder.image(binding["texture"], data)
        texture = self.builder.manifest["textures"][binding["texture"]]
        node.extension = "EXTEND" if texture.get("wrap", 0) == 1 else "REPEAT"
        node.interpolation = "Closest" if texture.get("filter", 1) == 0 else "Linear"
        self.assign(node.inputs[0], uv)
        self.sampled[key] = node
        self.used.add(prop)
        return node

    def normal_color(self, prop, packed=False):
        tex = self.sample(prop, True)
        storage = self.builder.manifest["textures"][self.textures[prop]["texture"]].get("storageFormat", "")
        if not packed and storage not in {"BC5", "DXT5", "BC3"} and prop != "_NormalBentMap":
            return tex.outputs["Color"]
        channels = self.separate(tex)
        x = channels[0]
        if storage in {"DXT5", "BC3"} or (packed and storage != "BC5" and prop != "_BumpMapUV2"):
            x = self.math("MULTIPLY", x, tex.outputs["Alpha"], "Unity 法线 X=R×A")
        node = self.node("ShaderNodeGroup", "RG 法线 · 重建 Z")
        node.node_tree = normal_tree()
        self.assign(node.inputs["X 编码"], x)
        self.assign(node.inputs["Y 编码"], channels[1])
        return node.outputs[0]

    def waves(self, tex):
        channels = self.separate(tex)
        emission = self.vectors.get("_EmissionColor", [1]*4)
        color = self.vectors.get("_MatEffectColor0", [0]*4)
        extra = self.vectors.get("_MatEffectParam2", [1, 0, 16, 16])
        terms = []
        for i in range(2):
            node = self.node("ShaderNodeGroup", "流光%d · %s区域 / %s相位" % (i+1, "RB"[i], "GA"[i]))
            node.node_tree = phase_tree()
            node["naraka_role"], node["naraka_flow_layer"] = "flow_layer", i
            param = self.vectors.get("_MatEffectParam%d" % i, [1, 0, 0, 1])
            tint = emission if i == 0 else [color[j]*(1+extra[1]*(emission[j]-1)) for j in range(3)]+[1]
            for name, value in {"区域蒙版": channels[i*2], "相位": channels[1] if i == 0 else tex.outputs["Alpha"],
                "时间 秒": self.time.outputs[0], "相位频率": param[0], "速度 rad/s": param[1],
                "阈值": min(param[2], 1-2**-24), "锐度": param[3], "颜色": tint, "强度": 1}.items():
                self.assign(node.inputs[name], value)
            terms.append(node.outputs["发光"])
        self.material["naraka_dynamic_material"] = "periodic2"
        return self.add_color(*terms, "两层相位流光相加")

    def effect(self):
        """Three simple texture layers; only UV motion, tint, mask and blend."""
        v, f = self.vectors, self.floats
        select = v.get("_TextureUVSelection", [0]*4)
        def layer(prop, index, tint_key):
            stem = prop
            st = v.get(stem+"ScaleOffset", [1,1,0,0])
            move = v.get(stem+("AnimateRotate" if index == 0 else "ScrollRotate"), [0]*4)
            uv = self.uv(1 if select[index] >= .5 else 0)
            mapping = self.node("ShaderNodeMapping", prop+" · UV滚动（单位/秒）")
            anchor = v.get(stem+"Anchor", [.5]*4)
            self.assign(mapping.inputs[0], self.vector("SUBTRACT", uv, (*anchor[2:], 0)))
            if "_TEXTURE_ROTATION_ON" in self.keywords:
                driver = mapping.inputs["Rotation"].driver_add("default_value", 2).driver
                for name, path in (("fps", "render.fps"), ("base", "render.fps_base")):
                    var = driver.variables.new(); var.name, var.type = name, "SINGLE_PROP"
                    var.targets[0].id_type, var.targets[0].id = "SCENE", bpy.context.scene
                    var.targets[0].data_path = path
                driver.expression = "(%r+frame*base/fps*(%r))*%r" % (move[2],move[3],math.pi/180)
            rotated = self.vector("ADD", mapping.outputs[0], (*anchor[2:],0))
            coords = self.vector("ADD", self.vector("MULTIPLY", self.vector("SUBTRACT", rotated, (*anchor[:2],0)), (*st[:2],1)), (*anchor[:2],0))
            # TextureEnv ST is applied by sample, separately from these vectors.
            shift = self.node("ShaderNodeMapping", prop+" · 平铺与时间偏移")
            self.assign(shift.inputs[0], coords)
            shift.inputs["Location"].default_value = (*st[2:],0)
            for axis in range(2):
                if move[axis]:
                    driver = shift.inputs["Location"].driver_add("default_value", axis).driver
                    for name,path in (("fps","render.fps"),("base","render.fps_base")):
                        var=driver.variables.new();var.name,var.type=name,"SINGLE_PROP"
                        var.targets[0].id_type,var.targets[0].id="SCENE",bpy.context.scene;var.targets[0].data_path=path
                    driver.expression="%r+frame*base/fps*(%r)"%(st[axis+2],move[axis])
            tex = self.sample(prop, False, uv_override=shift.outputs[0])
            tint = v.get(tint_key,[1]*4)
            return self.multiply_color(tex.outputs["Color"],tint,prop+" × 颜色"), self.math("MULTIPLY",tex.outputs["Alpha"],tint[3])
        rgb, alpha = layer("_MainTexture",0,"_MainTextureTint")
        if "_SECOND_LAYER_ON" in self.keywords and self.first(("_SecondTexture",)):
            second, second_a = layer("_SecondTexture",1,"_SecondTextureTintColor")
            blend=v.get("_LayerBlendParam",[0,1,0,1])
            merged=self.multiply_color(rgb,second,"图层相乘") if int(blend[0])==0 else self.add_color(rgb,second,"图层相加") if int(blend[0])==1 else second
            rgb=self.mix(rgb,merged,blend[1],"RGB 图层权重")
            merged_a=self.math("MULTIPLY",alpha,second_a) if int(blend[2])==0 else self.math("ADD",alpha,second_a) if int(blend[2])==1 else second_a
            alpha=self.mix(alpha,merged_a,blend[3],"Alpha 图层权重")
        if "_MASK_LAYER_ON" in self.keywords and self.first(("_MaskTexture",)):
            mask,mask_a=layer("_MaskTexture",2,"_MaskTextureTint")
            blend=v.get("_LayerBlendMaskParam",[1,1,0,0])
            rgb=self.multiply_color(rgb,self.mix((1,1,1,1),self.mix(mask,mask_a,blend[3],"Mask RGB/A"),blend[0],"RGB 遮罩权重"),"RGB 遮罩")
            bw=self.node("ShaderNodeRGBToBW","Mask 亮度")
            self.assign(bw.inputs[0],mask)
            alpha=self.math("MULTIPLY",alpha,self.mix(1,self.mix(mask_a,bw.outputs[0],blend[2],"Alpha 遮罩通道"),blend[1],"Alpha 遮罩权重"))
        vertex=self.node("ShaderNodeVertexColor","顶点色 × 特效");vertex.layer_name="Color"
        tint=[a*b for a,b in zip(v.get("_CustomDyeColor",[1]*4),v.get("_GeneralTintColor",[1]*4))]
        rgb=self.multiply_color(self.multiply_color(rgb,vertex.outputs["Color"],"特效顶点色"),tint,"特效总颜色")
        alpha=self.math("MULTIPLY",self.math("MULTIPLY",alpha,vertex.outputs["Alpha"]),tint[3])
        if f.get("_GeneralAlphaPremult",0):
            rgb=self.multiply_color(rgb,alpha,"特效预乘")
        return rgb,alpha

    def build(self):
        shader = self.source["shader"].lower().rsplit("/",1)[-1]
        from .advanced_cloth import VERIFIED_SHADERS
        advanced, skin = shader in VERIFIED_SHADERS, shader == "lx22standardcharacterskin.shader"
        self.section = "output"
        out = self.node("ShaderNodeOutputMaterial", "材质输出")
        bsdf = self.node("ShaderNodeBsdfPrincipled", "轻量 PBR")
        self.links.new(bsdf.outputs[0], out.inputs["Surface"])
        base = normal = None
        if shader == "fx_uber_transparent.shadergraph" and self.first(("_MainTexture",)):
            self.section="effect"
            rgb,alpha=self.effect()
            self.assign(bsdf.inputs["Base Color"],(0,0,0,1))
            self.assign(bsdf.inputs["Emission Color"],rgb);bsdf.inputs["Emission Strength"].default_value=1
        else:
            self.section="base"
            prop=self.first(("_MainTex","_BaseMap","_MainTexture","_DiffuseTexture","_BaseColorMap"))
            base=self.sample(prop)
            color=base.outputs["Color"] if base else self.vectors.get("_Color",[1]*4)
            self.section="surface"
            mr_prop=self.first(("_SpecGlossMap","_MetallicGlossMap","_MaskMap"))
            metal=self.floats.get("_Metallic",0)
            rough=self.floats.get("_Roughness",1-self.floats.get("_Glossiness",.5))
            if mr_prop:
                mr=self.sample(mr_prop,True);cs=self.separate(mr)
                if skin:
                    rough=self.math("SUBTRACT",1,self.math("POWER",cs[0],self.floats.get("_GlossMapScale",1)))
                    metal=self.vectors.get("_SkinColor",[0]*4)[3]*self.floats.get("_SkinMetallic",0)
                elif advanced or "periodic2" in shader or "_mrav" in self.builder.manifest["textures"][self.textures[mr_prop]["texture"]]["name"].lower():
                    metal,rough=cs[0],cs[1]
                elif mr_prop=="_MetallicGlossMap":
                    metal,rough=cs[0],self.math("SUBTRACT",1,mr.outputs["Alpha"])
            self.section="normal"
            nprop=self.first(("_BumpMap","_NormalMap","_NormalTexture","_NormalBentMap"))
            ncolor=self.normal_color(nprop,advanced) if nprop else None
            if "_USEUV2" in self.keywords and self.first(("_MainTexUV2",)):
                self.section="uv2"
                overlay=self.sample("_MainTexUV2")
                weight=overlay.outputs["Alpha"]
                color=self.mix(color,overlay.outputs["Color"],weight,"第二套底色 · A为叠加权重")
                if self.first(("_SpecGlossMapUV2",)):
                    cs2=self.separate(self.sample("_SpecGlossMapUV2",True))
                    metal=self.mix(metal,cs2[0],weight,"UV1 金属度叠加")
                    rough=self.mix(rough,cs2[1],weight,"UV1 粗糙度叠加")
                if ncolor is not None and self.first(("_BumpMapUV2",)):
                    second=self.normal_color("_BumpMapUV2",True)
                    # Encoded tangent normals: add XY, multiply Z, then normalize
                    # in Normal Map. Neutral (.5,.5,1) leaves the base unchanged.
                    c1=self.vector("SUBTRACT",self.vector("MULTIPLY",ncolor,(2,2,2)),(1,1,1))
                    c2=self.vector("SUBTRACT",self.vector("MULTIPLY",second,(2,2,2)),(1,1,1))
                    xy=self.vector("MULTIPLY",self.vector("ADD",c1,c2),(1,1,0))
                    z=self.vector("MULTIPLY",self.vector("MULTIPLY",c1,c2),(0,0,1))
                    merged=self.vector("ADD",self.vector("MULTIPLY",self.vector("ADD",xy,z),(.5,.5,.5)),(.5,.5,.5))
                    ncolor=self.mix(ncolor,merged,weight,"UV1 法线叠加")
            self.section="base"
            if base:
                color=self.multiply_color(color,self.vectors.get("_Color",self.vectors.get("_BaseColor",[1]*4)),"基础色 × 材质颜色")
            self.assign(bsdf.inputs["Base Color"],color)
            self.assign(bsdf.inputs["Metallic"],metal)
            if not skin and mr_prop and (advanced or "periodic2" in shader or "_mrav" in self.builder.manifest["textures"][self.textures[mr_prop]["texture"]]["name"].lower()):
                rough=self.math("ADD",1,self.math("MULTIPLY",self.math("SUBTRACT",rough,1),self.floats.get("_GlossMapScale",1)),"粗糙度强度")
            self.assign(bsdf.inputs["Roughness"],rough)
            if ncolor is not None:
                self.section="normal"
                normal=self.node("ShaderNodeNormalMap","切线法线 · UV0");normal.uv_map="UV0"
                self.assign(normal.inputs["Color"],ncolor)
                normal.inputs["Strength"].default_value=self.floats.get("_BumpScale",1)
                self.assign(bsdf.inputs["Normal"],normal.outputs[0])
            self.section="emission"
            emprop=self.first(("_EmissionMap","_EmissionTexture","_EmissiveMap"))
            periodic=(advanced or "periodic2" in shader) and (self.floats.get("_MFX_EMISSIVE_PERIODIC_2",0) or "_MFX_EMISSIVE_PERIODIC_2" in self.keywords or "_MFX_EMISSIVE_PERIODIC_2_ON" in self.keywords)
            # This saved billboard branch never samples its bound em map.
            disabled_billboard=shader=="ch_cloth_billboard_opaque.shadergraph" and not self.keywords & {"_MFX_EMISSIVE_PERIODIC_2","_MFX_EMISSIVE_PERIODIC_2_ON"}
            if emprop and not disabled_billboard:
                coords=None
                if advanced:
                    chosen=[]
                    for i in range(3):
                        if int(self.floats.get("_EffectUvSelector",1)) & (1<<i):chosen.append(self.uv(i))
                    coords=chosen[0] if chosen else (0,0,0)
                    for uv in chosen[1:]:coords=self.vector("ADD",coords,uv)
                    transform=self.vectors.get("_EffectUVTransform",[0,0,1,1])
                    if transform != [0,0,1,1]:coords=self.vector("DIVIDE",self.vector("SUBTRACT",coords,(*transform[:2],0)),(*transform[2:],1))
                tex=self.sample(emprop,True if periodic else self.builder.manifest["textures"][self.textures[emprop]["texture"]]["linear"],uv_override=coords)
                emission=self.waves(tex) if periodic else self.multiply_color(tex.outputs["Color"],self.vectors.get("_EmissionColor",[1]*4),"发光图 × 材质颜色")
                self.assign(bsdf.inputs["Emission Color"],emission);bsdf.inputs["Emission Strength"].default_value=1
            alpha=1
        self.section="alpha"
        policy=resolve_alpha(self.source)
        if policy["depthOnly"]:alpha=0
        elif base and policy["useBaseAlpha"]:
            alpha=self.math("MULTIPLY",base.outputs["Alpha"],self.vectors.get("_Color",[1]*4)[3],"材质启用的透明度")
            if policy["silkCoverage"]:alpha=self.saturate(self.math("DIVIDE",alpha,max(.01,policy["cutoff"])),0,1)
            if policy["cutout"]:alpha=self.math("SUBTRACT",1,self.math("LESS_THAN",alpha,policy["cutoff"]),"材质裁切阈值")
        if policy["cull"] in {1,2}:
            geo=self.node("ShaderNodeNewGeometry","正反面剔除")
            visibility=geo.outputs["Backfacing"] if policy["cull"]==1 else self.math("SUBTRACT",1,geo.outputs["Backfacing"])
            alpha=self.math("MULTIPLY",alpha,visibility)
        self.assign(bsdf.inputs["Alpha"],alpha)
        if shader=="fx_uber_transparent.shadergraph" and self.floats.get("_SrcBlend")==1 and self.floats.get("_DstBlend")==1:
            self.section="output"
            emission=self.node("ShaderNodeEmission","加法特效发光")
            self.assign(emission.inputs[0],self.multiply_color(rgb,alpha,"加法特效覆盖"))
            transparent=self.node("ShaderNodeBsdfTransparent","透过后方物体")
            add=self.node("ShaderNodeAddShader","One / One 加法")
            self.links.new(transparent.outputs[0],add.inputs[0]);self.links.new(emission.outputs[0],add.inputs[1]);self.links.new(add.outputs[0],out.inputs["Surface"])
        self.material.use_backface_culling=policy["cull"]==2
        if hasattr(self.material,"surface_render_method"):self.material.surface_render_method="DITHERED"
        else:self.material.blend_method="HASHED"
        self.material["naraka_material_profile"]="LIGHTWEIGHT"
        self.material["naraka_readable_layout"]=2
        self.notes.append("Lightweight PBR: no detail arrays, diamond/glitter, refraction, view-dependent emission, detail emission gates or original multi-pass coverage. Source parameters remain metadata; unused images are not loaded.")
        status={"surface":"approximate","profile":"LIGHTWEIGHT","alpha":policy,"verifiedFeatures":["PBR channel routing","packed RG/BA phase waves","source UV sets and UV motion"],"retainedTextureBindings":[],"omittedTextureBindings":[p for p,b in self.textures.items() if b.get("texture") and p not in self.used],"notes":self.notes}
        self.material["naraka_conversion_status_json"]=json.dumps(status,ensure_ascii=False)
        self.material["naraka_conversion_notes"]=json.dumps(self.notes,ensure_ascii=False)
        self.builder.statuses[self.source["id"]]=status
        self.builder.adapters[self.source["id"]]="lightweight-pbr"
        self.builder.report.extend(self.source["name"]+": "+n for n in self.notes)
        keep=set();stack=[out]
        while stack:
            node=stack.pop()
            if node in keep:continue
            keep.add(node);stack.extend(l.from_node for s in node.inputs for l in s.links)
        for node in list(self.nodes):
            if node not in keep:self.nodes.remove(node)
        from .texture_layout import expose
        expose(self.material.node_tree,self.source)
        quiet_previews(self.material.node_tree)
        arrange_lightweight(self.material)
        return self.material
