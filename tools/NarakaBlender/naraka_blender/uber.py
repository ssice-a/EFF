"""fx_uber_transparent's texture/layer path (D3D11 variant 925).

Depth soft-particles, ocean clipping, fog and game exposure are scene-dependent
and intentionally left to Blender's scene. Particle custom-data needs simulation.
"""
import math
from .advanced_cloth import AdvancedCloth


def build_uber(g):
    a = AdvancedCloth(g)
    M,V = a.M,a.V
    v,f = g.vectors,g.floats
    g.section = "effect"
    selections = v.get("_TextureUVSelection", [0]*4)
    def uv(index):
        return a.uv(1 if selections[index] >= .5 else 0)
    def scaled(coord, st, anchor):
        return V("ADD", V("MULTIPLY", V("SUBTRACT",coord,(*anchor,0)),(*st[:2],1)),(*anchor,0))
    def animated(coord, st, movement, anchor, distorted=(0,0,0)):
        if g.keywords & {"_TEXTURE_ROTATION_ON"}:
            rotation = M("MULTIPLY",M("ADD",movement[2],M("MULTIPLY",g.time.outputs[0],movement[3])),math.pi/180)
            xy = a.split(V("SUBTRACT",coord,(*anchor[2:],0)))
            s,c = M("SINE",rotation),M("COSINE",rotation)
            coord = V("ADD",a.vec((M("SUBTRACT",M("MULTIPLY",xy[0],c),M("MULTIPLY",xy[1],s)),M("ADD",M("MULTIPLY",xy[0],s),M("MULTIPLY",xy[1],c)),0)),(*anchor[2:],0))
        result = scaled(coord,st,anchor[:2])
        shift = a.vec((M("ADD",st[2],M("MULTIPLY",g.time.outputs[0],movement[0])),M("ADD",st[3],M("MULTIPLY",g.time.outputs[0],movement[1])),0))
        return V("ADD",V("ADD",result,shift),distorted)
    distortion = (0,0,0)
    if "_UV_DISTORT_ON" in g.keywords and g.first(("_DistortTexture",)):
        st,scroll = v.get("_DistortScaleOffset",[1,1,0,0]),v.get("_DistortScrollAnchor",[0,0,.5,.5])
        duv = scaled(uv(3),st,scroll[2:])
        duv = V("ADD",duv,a.vec((M("ADD",st[2],M("MULTIPLY",g.time.outputs[0],scroll[0])),M("ADD",st[3],M("MULTIPLY",g.time.outputs[0],scroll[1])),0)))
        tex = g.sample("_DistortTexture",True,uv_override=duv)
        cs = g.separate(tex)
        red = a.lerp(cs[0],M("MULTIPLY",cs[0],tex.outputs["Alpha"]),f.get("_DistortIsNormal",0))
        distortion = a.vec((M("SUBTRACT",M("MULTIPLY",red,2),1),M("SUBTRACT",M("MULTIPLY",cs[1],2),1),0),"Distortion RG / normal A")
    def layer(prop,index,tint_name,falloff,distort):
        stem = prop
        st = v.get(stem+"ScaleOffset",[1,1,0,0])
        move = v.get(stem+("AnimateRotate" if prop=="_MainTexture" else "ScrollRotate"),[0]*4)
        anchor = v.get(stem+"Anchor",[.5]*4)
        coords = animated(uv(index),st,move,anchor,V("MULTIPLY",distortion,(*distort,0)))
        if f.get(stem+"ForceClamp",0):
            xyz = a.split(coords)
            coords = a.vec((g.saturate(xyz[0],.01,.99),g.saturate(xyz[1],.01,.99),0))
        identity = g.textures[prop]["texture"]
        tex = g.sample(prop,g.builder.manifest["textures"][identity]["linear"],uv_override=coords)
        tint = v.get(tint_name,[1]*4)
        rgb = g.multiply_color(tex.outputs["Color"],tint,prop+" source tint")
        lum = g.saturate(V("DOT_PRODUCT",rgb,(.21,.72,.07)))
        lum = M("MAXIMUM",lum,.0001)
        rgb = g.multiply_color(rgb,M("DIVIDE",M("POWER",lum,falloff[0]),lum),prop+" luma falloff")
        alpha = M("POWER",M("MAXIMUM",M("MULTIPLY",tex.outputs["Alpha"],tint[3]),0),falloff[1])
        return rgb,alpha
    falloff = v.get("_MainSecondRGBAFalloff",[1]*4)
    distort = v.get("_DistortMainSecond",[0]*4)
    rgb,alpha = layer("_MainTexture",0,"_MainTextureTint",falloff[:2],distort[:2])
    if "_SECOND_LAYER_ON" in g.keywords and g.first(("_SecondTexture",)):
        second,second_a = layer("_SecondTexture",1,"_SecondTextureTintColor",falloff[2:],distort[2:])
        blend = v.get("_LayerBlendParam",[0,1,0,1])
        # Shader's icb: 0 multiply, 1 add, 2 replace.
        merged = g.multiply_color(rgb,second,"Multiply effect layers") if int(blend[0])==0 else g.add_color(rgb,second,"Add effect layers") if int(blend[0])==1 else second
        rgb = g.mix(rgb,merged,blend[1],"Effect RGB blend weight")
        merged_a = M("MULTIPLY",alpha,second_a) if int(blend[2])==0 else M("ADD",alpha,second_a) if int(blend[2])==1 else second_a
        alpha = g.saturate(a.lerp(alpha,merged_a,blend[3]))
    if "_MASK_LAYER_ON" in g.keywords and g.first(("_MaskTexture",)):
        mask,mask_a = layer("_MaskTexture",2,"_MaskTextureTint",v.get("_MaskRGBAFalloff",[1,1])[:2],v.get("_DistortMaskNormal",[0]*4)[:2])
        blend = v.get("_LayerBlendMaskParam",[1,1,0,0])
        rgb = g.multiply_color(rgb,g.mix((1,1,1,1),g.mix(mask,mask_a,blend[3],"Mask RGB / A selector"),blend[0],"Mask color intensity"),"Masked effect RGB")
        mask_lum = V("DOT_PRODUCT",mask,(.21,.72,.07))
        alpha = M("MULTIPLY",alpha,a.lerp(1,a.lerp(mask_a,mask_lum,blend[2]),blend[1]))
    vertex = g.node("ShaderNodeVertexColor","Effect vertex color")
    vertex.layer_name = "Color"
    tint = [x*y for x,y in zip(v.get("_CustomDyeColor",[1]*4),v.get("_GeneralTintColor",[1]*4))]
    rgb = g.multiply_color(g.multiply_color(rgb,vertex.outputs["Color"],"Effect vertex RGB"),tint,"General effect tint")
    alpha = M("MULTIPLY",M("MULTIPLY",alpha,vertex.outputs["Alpha"]),tint[3])
    gamma = v.get("_GeneralGamma",[1]*4)
    cs = a.split(rgb)
    rgb = a.vec(tuple(M("POWER",M("MAXIMUM",cs[i],0),gamma[i]) for i in range(3)),"Source effect gamma")
    alpha = M("POWER",M("MAXIMUM",alpha,0),gamma[3])
    rgb = g.multiply_color(rgb,a.lerp(1,alpha,f.get("_GeneralAlphaPremult",0)),"Source effect premultiplication")
    g.verified.append("fx_uber: UV selection/rotation/scroll/distortion, texture tints, layer modes, mask, gamma, premultiplication")
    g.notes.append("Uber scene-depth soft fade, game fog/exposure, ocean clipping and particle custom data are not reproduced")
    return rgb,g.saturate(M("MULTIPLY",alpha,f.get("_GeneralAlphaExtinction",1)))
