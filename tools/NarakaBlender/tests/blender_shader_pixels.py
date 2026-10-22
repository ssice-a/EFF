"""Render the real adapter with controlled packed data; compare linear pixels.

Run with Blender 5 --background --factory-startup --python-exit-code 1.
No screenshots or node-name assertions substitute for the pixel checks.
"""
import copy
import json
import math
import sys
import tempfile
from array import array
from pathlib import Path

import bpy

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from naraka_blender.materials import MaterialBuilder

args=sys.argv[sys.argv.index('--')+1:]
package=Path(args[0]); output=Path(args[1]);output.mkdir(parents=True,exist_ok=True)
manifest=json.loads(package.read_text(encoding='utf-8-sig'))
source=copy.deepcopy(next(m for m in manifest['materials'].values() if 'body02_lv_s3'==m['name'].removeprefix('ch_f_ming_wuzhen_')))
source['vectors'].update(_EmissionColor=[2,.1,4,1],_MatEffectColor0=[.3,1,.2,.1],
    _MatEffectParam0=[1,2,-.1,2],_MatEffectParam1=[2,-1,0,3],_MatEffectParam2=[1,.25,16,16],_ShaderFeatureParam1=[0]*4)
source['keywords']='_MFX_EMISSIVE_PERIODIC_2 _UseDetail3'
packed=[.8,.9,.5,.2]
values={'_MainTex':[.4,.4,.4,1],'_BumpMap':[.5,.5,1,1],'_SpecGlossMap':[.2,.3,1,1],
    '_EmissionMap':packed,'_AdvIDTex':[0,0,0,0],'_TillingChannelMask':[0,0,0,0],
    '_DetailTillingArray':[.5,.5,.7,1]}
by_id={binding['texture']:values[prop] for prop,binding in source['textures'].items() if binding.get('texture') and prop in values}

class FixtureBuilder(MaterialBuilder):
    def image(self,identity,linear,slice_index=None):
        key=identity,linear,slice_index
        if key not in self.images:
            img=bpy.data.images.new('Controlled packed pixel',width=1,height=1,alpha=True,float_buffer=True)
            img.colorspace_settings.name='Non-Color';img.alpha_mode='CHANNEL_PACKED'
            img.pixels[:]=by_id.get(identity,[.5]*4)
            self.images[key]=img
        return self.images[key]

builder=FixtureBuilder(package.parent,manifest,[],profile='REFERENCE')
mat=builder.build(source)
principled=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
out=next(n for n in mat.node_tree.nodes if n.type=='OUTPUT_MATERIAL')
emission=mat.node_tree.nodes.new('ShaderNodeEmission')
mat.node_tree.links.new(principled.inputs['Emission Color'].links[0].from_socket,emission.inputs[0])
mat.node_tree.links.new(emission.outputs[0],out.inputs['Surface'])
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.mesh.primitive_plane_add(size=2)
plane=bpy.context.object;plane.data.uv_layers.active.name='UV0';plane.data.materials.append(mat)
bpy.ops.object.camera_add(location=(0,0,3));cam=bpy.context.object;cam.data.type='ORTHO';cam.data.ortho_scale=1
scene=bpy.context.scene;scene.camera=cam;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=1;scene.cycles.use_denoising=False
scene.render.resolution_x=8;scene.render.resolution_y=8;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='OPEN_EXR';scene.render.image_settings.color_mode='RGBA'
scene.render.film_transparent=True
def render(frame,label):
    scene.frame_set(frame);scene.render.filepath=str(output/(label+'.exr'));bpy.ops.render.render(write_still=True)
    image=bpy.data.images.load(scene.render.filepath,check_existing=False)
    pixels=array('f',[0])*len(image.pixels);image.pixels.foreach_get(pixels)
    result=[sum(pixels[c::4])/64 for c in range(3)]
    bpy.data.images.remove(image)
    return result
def expected(frame):
    t=frame*scene.render.fps_base/scene.render.fps
    terms=[]
    for i,phase in enumerate((packed[1],packed[3])):
        p=source['vectors']['_MatEffectParam%d'%i]
        wave=max(0,min(1,((math.sin(phase*p[0]+t*p[1])*.5+.5)-p[2])/(1-p[2])))**p[3]
        terms.append(packed[i*2]**2*wave)
    e=source['vectors']['_EmissionColor'];c=source['vectors']['_MatEffectColor0']
    return [terms[0]*e[j]+terms[1]*c[j]*(1+.25*(e[j]-1)) for j in range(3)]
for frame in (0,37):
    actual=render(frame,'periodic-%d'%frame);want=expected(frame)
    assert max(abs(a-b) for a,b in zip(actual,want))<.003,(frame,actual,want)
    assert actual[2]>actual[1]*3,(frame,actual)
    print('PIXEL PASS Periodic2',frame,actual,'expected',want)
# A full detail mask with disabled effect bits must suppress periodic emission.
mask_id=source['textures']['_TillingChannelMask']['texture']
source['floats']['_EffectByDetailMap']=0
by_id[mask_id]=[0,0,1,0]
builder=FixtureBuilder(package.parent,manifest,[],profile='REFERENCE');gated=builder.build(source)
p=next(n for n in gated.node_tree.nodes if n.type=='BSDF_PRINCIPLED');o=next(n for n in gated.node_tree.nodes if n.type=='OUTPUT_MATERIAL')
e=gated.node_tree.nodes.new('ShaderNodeEmission');gated.node_tree.links.new(p.inputs['Emission Color'].links[0].from_socket,e.inputs[0]);gated.node_tree.links.new(e.outputs[0],o.inputs['Surface'])
plane.data.materials[0]=gated
actual=render(0,'detail-gated')
assert max(actual)<.0001,actual
print('PIXEL PASS tiling B / array layer 3 / EffectByDetailMap gate',actual)
print('BLENDER_SHADER_PIXELS_PASS',bpy.app.version_string)
