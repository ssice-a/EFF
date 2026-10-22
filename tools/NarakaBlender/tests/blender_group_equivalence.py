"""Compare the full surface before/after DAG grouping, in linear EXR pixels."""
import json
import sys
from array import array
from pathlib import Path

import bpy
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from naraka_blender.materials import MaterialBuilder

args=sys.argv[sys.argv.index('--')+1:];p=Path(args[0]);output=Path(args[1]);output.mkdir(parents=True,exist_ok=True)
f=json.loads(p.read_text('utf-8-sig'))
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.mesh.primitive_plane_add(size=2);plane=bpy.context.object;plane.data.uv_layers.active.name='UV0'
second=plane.data.uv_layers.new(name='UV1')
for a,b in zip(plane.data.uv_layers['UV0'].data,second.data):b.uv=a.uv
color=plane.data.color_attributes.new(name='Color',type='FLOAT_COLOR',domain='POINT')
for datum in color.data:datum.color=(1,1,1,1)
bpy.ops.object.camera_add(location=(0,0,3));camera=bpy.context.object;camera.data.type='ORTHO';camera.data.ortho_scale=1
s=bpy.context.scene;s.camera=camera;s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=1;s.cycles.use_denoising=False
s.world.color=(.1,.1,.1);s.render.resolution_x=8;s.render.resolution_y=8;s.render.resolution_percentage=100
s.render.image_settings.file_format='OPEN_EXR';s.render.image_settings.color_mode='RGBA'
for material_name in ('ch_f_ming_wuzhen_body02_lv_s3','ch_f_ming_wuzhen_cloth01_cb_lv_s3','ch_f_ming_wuzhen_fx_body01_lv_s3'):
    source=next(m for m in f['materials'].values() if m['name']==material_name)
    old=MaterialBuilder(p.parent,f,[],readable=False,profile='REFERENCE').build(source)
    new=MaterialBuilder(p.parent,f,[],readable=True,profile='REFERENCE').build(source)
    for frame in (1,37):
        samples=[]
        for tag,m in [('expanded',old),('grouped',new)]:
            plane.data.materials.clear();plane.data.materials.append(m);s.frame_set(frame)
            s.render.filepath=str(output/(material_name+'-'+str(frame)+'-'+tag+'.exr'))
            bpy.ops.render.render(write_still=True)
            image=bpy.data.images.load(s.render.filepath,check_existing=False);pixels=array('f',[0])*len(image.pixels)
            image.pixels.foreach_get(pixels);samples.append(pixels);bpy.data.images.remove(image)
        error=max(abs(a-b) for a,b in zip(*samples))
        assert error<.00001,(material_name,frame,error)
        print('GROUP PIXEL EQUIVALENCE PASS',material_name,frame,error)
print('BLENDER_GROUP_EQUIVALENCE_PASS')
