"""The portable flow contract must be visible at the material's top level."""
import json
import sys
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from naraka_blender.materials import MaterialBuilder
from naraka_blender.readable import set_preview

p=Path(sys.argv[sys.argv.index('--')+1]);f=json.loads(p.read_text('utf-8-sig'))
source=next(m for m in f['materials'].values() if m['name']=='ch_f_ming_wuzhen_cloth01_cb_lv_s3')
m=MaterialBuilder(p.parent,f,[],profile='REFERENCE').build(source)
nodes=[n for n in m.node_tree.nodes if n.type!='FRAME']
expected={k for k,v in source['textures'].items() if v.get('texture')}
visible={n.get('naraka_binding') for n in nodes if n.type=='TEX_IMAGE'}
assert expected<=visible,('Texture bindings hidden inside groups',sorted(expected-visible))
def hidden_images(tree):
    return [n for n in tree.nodes if n.type=='TEX_IMAGE']+[t for n in tree.nodes if n.type=='GROUP' for t in hidden_images(n.node_tree)]
assert sum(n.type=='TEX_IMAGE' for n in nodes)==len(hidden_images(m.node_tree)), 'Every actual texture sample must stay at root'
for n in nodes:
    if n.type=='TEX_IMAGE':
        assert n.parent and n.parent.type=='FRAME'
        assert n.get('naraka_texture_usage') and n.get('naraka_uv_usage')
for prop in ('_AdvIDTex','_TillingChannelMask','_SpecGlossMap','_EmissionMap'):
    assert any(n.type=='SEPARATE_COLOR' and n.get('naraka_binding')==prop for n in nodes),prop
waves=[n for n in nodes if n.get('naraka_role')=='flow_layer']
assert len(waves)==2
for wave in waves:
    assert {'区域蒙版','相位','时间 秒','相位频率','速度 rad/s','阈值','锐度','颜色','强度'}<=set(s.name for s in wave.inputs)
    assert {'发光','移动亮区','区域蒙版','相位'}<=set(s.name for s in wave.outputs)
    assert wave.inputs['速度 rad/s'].default_value==2
    assert wave.inputs['相位频率'].default_value==10
assert any(n.type=='TEX_IMAGE' and n.label.startswith('流光数据') for n in nodes)
out=next(n for n in m.node_tree.nodes if n.type=='OUTPUT_MATERIAL')
original=out.inputs['Surface'].links[0].from_socket
for mode in ('MASK_1','PHASE_1','WAVE_1','COLOR_2','EMISSION','NORMAL'):
    set_preview(m,mode)
    assert out.inputs['Surface'].is_linked
assert out.inputs['Surface'].links[0].from_socket==original
assert not any(n.get('naraka_role')=='debug_preview' for n in m.node_tree.nodes)
print('READABLE_MATERIAL_PASS',len(nodes),'top-level nodes; every texture and source channel visible; two parameterized flow layers')
