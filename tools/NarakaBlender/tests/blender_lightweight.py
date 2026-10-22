"""Check real-package complexity, packed-wave pixels, UV routing and migration."""
import copy
import json
import math
import sys
import time
from array import array
from pathlib import Path

import bpy

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from naraka_blender.materials import MaterialBuilder
from naraka_blender.readable import set_preview
from naraka_blender.layout import arrange_material, node_size
from naraka_blender.simplify import simplify_materials, cleanup_unused_legacy

args=sys.argv[sys.argv.index('--')+1:]
package=Path(args[0]);output=Path(args[1]);output.mkdir(parents=True,exist_ok=True)
manifest=json.loads(package.read_text('utf-8-sig'))
builder=MaterialBuilder(package.parent,manifest,[])
started=time.perf_counter()
mats={key:builder.build(source) for key,source in manifest['materials'].items()}

def cost(tree):
    return len(tree.nodes)+sum(cost(n.node_tree) for n in tree.nodes if n.type=='GROUP')

def uv_sources(socket):
    nodes=set();stack=[l.from_node for l in socket.links]
    while stack:
        n=stack.pop()
        if n in nodes:continue
        nodes.add(n);stack.extend(l.from_node for s in n.inputs for l in s.links)
    return {n.uv_map for n in nodes if n.type=='UVMAP'}

metrics=[]
for identity,mat in mats.items():
    source=manifest['materials'][identity]
    assert mat['naraka_material_profile']=='LIGHTWEIGHT'
    nodes=list(mat.node_tree.nodes)
    assert not any(n.type=='TEX_IMAGE' and n.get('naraka_binding') in {'_DetailTillingArray','_TillingChannelMask','_AdvIDTex'} for n in nodes)
    assert len(nodes)<95,(mat.name,len(nodes))
    assert cost(mat.node_tree)<150,(mat.name,cost(mat.node_tree))
    for node in nodes:
        if node.type=='GROUP':assert not any(n.type=='TEX_IMAGE' for n in node.node_tree.nodes)
    em=next((n for n in nodes if n.type=='TEX_IMAGE' and n.get('naraka_binding')=='_EmissionMap'),None)
    if em and 'clothadvanced' in source['shader']:
        selector=int(source['floats']['_EffectUvSelector'])
        assert uv_sources(em.inputs[0])=={'UV1' if selector==2 else 'UV0'}
    if source['name']=='ch_f_ming_wuzhen_billboard_lv_s3':assert em is None
    if '_USEUV2' in source.get('keywords',''):
        overlay=next(n for n in nodes if n.type=='TEX_IMAGE' and n.get('naraka_binding')=='_MainTexUV2')
        assert uv_sources(overlay.inputs[0])=={'UV1'}
        assert any(l.from_node==overlay and l.from_socket==overlay.outputs[1] for l in mat.node_tree.links)
    # Arrange is connection-preserving and leaves no estimated node overlaps.
    links=[(l.from_node,l.from_socket,l.to_node,l.to_socket) for l in mat.node_tree.links]
    arrange_material(mat)
    assert links==[(l.from_node,l.from_socket,l.to_node,l.to_socket) for l in mat.node_tree.links]
    for i,a in enumerate(nodes):
        aw,ah=node_size(a)
        for b in nodes[i+1:]:
            bw,bh=node_size(b)
            assert a.location.x+max(aw,a.width)<=b.location.x or b.location.x+max(bw,b.width)<=a.location.x or a.location.y-ah>=b.location.y or b.location.y-bh>=a.location.y,(mat.name,a.label,b.label)
    metrics.append({'name':mat.name,'rootNodes':len(nodes),'expandedNodes':cost(mat.node_tree),'samplers':sum(n.type=='TEX_IMAGE' for n in nodes)})
metrics_time=time.perf_counter()-started

source=copy.deepcopy(next(s for s in manifest['materials'].values() if s['name']=='ch_f_ming_wuzhen_body02_lv_s3'))
source['vectors'].update(_EmissionColor=[2,.1,4,1],_MatEffectColor0=[.3,1,.2,.1],_MatEffectParam0=[1,2,-.1,2],_MatEffectParam1=[2,-1,0,3],_MatEffectParam2=[1,.25,16,16])
packed=[.8,.9,.5,.2]
em_id=source['textures']['_EmissionMap']['texture']
class Fixture(MaterialBuilder):
    def image(self,identity,linear,slice_index=None):
        key=identity,linear,slice_index
        if key not in self.images:
            image=bpy.data.images.new('Test packed pixel',width=1,height=1,alpha=True,float_buffer=True)
            image.colorspace_settings.name='Non-Color';image.alpha_mode='CHANNEL_PACKED'
            image.pixels[:]=packed if identity==em_id else [.5,.5,1,1]
            self.images[key]=image
        return self.images[key]
mat=Fixture(package.parent,manifest,[]).build(source)
tree=mat.node_tree;bsdf=next(n for n in tree.nodes if n.type=='BSDF_PRINCIPLED');out=next(n for n in tree.nodes if n.type=='OUTPUT_MATERIAL')
original=out.inputs['Surface'].links[0].from_socket
for mode in ('MASK_1','PHASE_2','WAVE_1','COLOR_2','EMISSION','NORMAL'):set_preview(mat,mode)
assert out.inputs['Surface'].links[0].from_socket==original
emission=tree.nodes.new('ShaderNodeEmission')
tree.links.new(bsdf.inputs['Emission Color'].links[0].from_socket,emission.inputs[0]);tree.links.new(emission.outputs[0],out.inputs[0])
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.mesh.primitive_plane_add(size=2)
plane=bpy.context.object;plane.data.uv_layers.active.name='UV0';plane.data.materials.append(mat)
bpy.ops.object.camera_add(location=(0,0,3));camera=bpy.context.object;camera.data.type='ORTHO';camera.data.ortho_scale=1
scene=bpy.context.scene;scene.camera=camera;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=1;scene.cycles.use_denoising=False
scene.render.resolution_x=8;scene.render.resolution_y=8;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='OPEN_EXR';scene.render.image_settings.color_mode='RGBA';scene.render.film_transparent=True
pixels=[]
for frame in (0,37):
    scene.frame_set(frame);scene.render.filepath=str(output/('wave-%d.exr'%frame));bpy.ops.render.render(write_still=True)
    image=bpy.data.images.load(scene.render.filepath,check_existing=False);data=array('f',[0])*len(image.pixels);image.pixels.foreach_get(data)
    actual=[sum(data[c::4])/64 for c in range(3)];bpy.data.images.remove(image)
    t=frame*scene.render.fps_base/scene.render.fps;terms=[]
    for i,phase in enumerate((packed[1],packed[3])):
        p=source['vectors']['_MatEffectParam%d'%i]
        terms.append(packed[i*2]**2*max(0,min(1,(math.sin(phase*p[0]+t*p[1])*.5+.5-p[2])/(1-p[2])))**p[3])
    want=[terms[0]*source['vectors']['_EmissionColor'][j]+terms[1]*source['vectors']['_MatEffectColor0'][j]*(1+.25*(source['vectors']['_EmissionColor'][j]-1)) for j in range(3)]
    assert max(abs(a-b) for a,b in zip(actual,want))<.003,(frame,actual,want)
    pixels.append({'frame':frame,'actual':actual,'expected':want})

# Texture UV movement remains tied to seconds, and survives file save/reopen.
fx=next(m for m in mats.values() if 'fx_body01' in m.name)
plane.data.materials[0]=fx
positions=[]
for frame in (1,37):
    scene.frame_set(frame)
    positions.append([(n.label,list(n.inputs['Location'].default_value)) for n in fx.node_tree.nodes if n.type=='MAPPING' and '时间偏移' in n.label])
assert positions[0]!=positions[1],positions

# Existing material migration leaves vertex data, object transforms and unrelated
# materials intact, and backs up old node trees outside the active blend file.
old_source=copy.deepcopy(source);old_source['name']='Legacy migration fixture'
old=MaterialBuilder(package.parent,manifest,[],profile='REFERENCE').build(old_source)
unused=old.copy();unused.name='Unused old imported material';unused.use_fake_user=True
plane.data.materials[0]=old
old.name='Legacy migration fixture'
before_verts=[v.co[:] for v in plane.data.vertices];before_matrix=plane.matrix_world.copy()
result=simplify_materials([plane],output/'backups')
assert result['materials']==1 and Path(result['backup']).is_file()
assert [v.co[:] for v in plane.data.vertices]==before_verts and plane.matrix_world==before_matrix
assert plane.active_material['naraka_material_profile']=='LIGHTWEIGHT'
assert simplify_materials([plane],output/'backups')['materials']==0
with bpy.data.libraries.load(result['backup']) as (available,requested):
    assert 'Legacy migration fixture' in available.materials
cleanup=cleanup_unused_legacy(output/'backups')
assert cleanup['materials']==1 and Path(cleanup['backup']).is_file()
assert bpy.data.materials.get('Unused old imported material') is None
for m in mats.values():m.use_fake_user=True
save=output/'lightweight-verification.blend';bpy.ops.wm.save_as_mainfile(filepath=str(save))
bpy.ops.wm.open_mainfile(filepath=str(save))
assert all(m['naraka_material_profile']=='LIGHTWEIGHT' for m in bpy.data.materials if m.get('naraka_source'))
scene=bpy.context.scene
for frame in (1,37):
    scene.frame_set(frame)
    assigned={slot.material for obj in scene.objects for slot in getattr(obj,'material_slots',[])}
    for m in assigned:
        if m and m.get('naraka_dynamic_material')=='periodic2':
            time_node=next(n for n in m.node_tree.nodes if n.type=='VALUE')
            assert abs(time_node.outputs[0].default_value-frame*scene.render.fps_base/scene.render.fps)<1e-5
report={'materials':metrics,'materialBuildSeconds':metrics_time,'texturesLoaded':len(builder.images),'pixels':pixels,'scrollPositions':positions,'migration':result,'cleanup':cleanup,'saveReopen':True}
(output/'verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf-8')
print('LIGHTWEIGHT_MATERIAL_PASS',json.dumps(report,ensure_ascii=False))
