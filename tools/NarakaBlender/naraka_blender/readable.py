"""Readable material stages and a reusable, engine-independent periodic wave.

Grouping preserves socket types, image identities, drivers and evaluated math.
It is an editing aid, not a claim that groups reduce GPU shader instructions.
"""
import bpy


def quiet_previews(tree):
    props=getattr(tree,'node_preview',None)
    if props is not None:
        props.enabled=False
    for n in tree.nodes:
        n.show_preview=False
        props=getattr(n,'node_preview',None)
        if props is not None:
            props.enabled=False


def interface(tree,name,direction,kind,default=None,description=''):
    s=tree.interface.new_socket(name=name,in_out=direction,socket_type=kind)
    s.description=description
    if default is not None:
        s.default_value=default
    return s


def wave_tree():
    for tree in bpy.data.node_groups:
        if tree.get('naraka_group_contract')=='periodic-wave-v1':
            return tree
    tree=bpy.data.node_groups.new('永劫 · 相位推进流光','ShaderNodeTree')
    tree['naraka_group_contract']='periodic-wave-v1'
    specifications=[('区域蒙版',1,'R / B：固定哪些位置允许发光'),('相位',0,'G / A：定义亮区经过各处的先后次序'),
        ('时间 秒',0,'秒；不是帧数'),('相位频率',1,'phase * x；越大，相位图内波段越密'),
        ('速度 rad/s',0,'time * y；改变符号反转推进方向，完整周期为 2π/abs(y) 秒'),
        ('阈值',0,'z；用 (wave-z)/(1-z) 改变亮区宽度'),('锐度',1,'w；pow(mask,w)，控制亮区收缩'),
        ('视角点积',1,'Normal dot view；AdvancedCloth 使用绝对值'),('视角参数',16,'<16 正视更亮；>16 边缘更亮；16 不受视角影响'),
        ('强度',1,'额外编辑倍率；默认 1 保留源效果')]
    for name,default,description in specifications:
        s=interface(tree,name,'INPUT','NodeSocketFloat',default,description)
        if name in ('区域蒙版','相位'):s.min_value=0;s.max_value=1
        elif name in ('强度','锐度'):s.min_value=0;s.max_value=1000
    interface(tree,'颜色','INPUT','NodeSocketColor',(1,1,1,1),'源材质的线性 HDR 颜色；不来自 packed 图的 RGB')
    for name,kind in [('发光','NodeSocketColor'),('移动亮区','NodeSocketFloat'),('区域蒙版','NodeSocketFloat'),('相位','NodeSocketFloat')]:
        interface(tree,name,'OUTPUT',kind)
    ns,ls=tree.nodes,tree.links
    inp=ns.new('NodeGroupInput');out=ns.new('NodeGroupOutput')
    inp.location=(-1000,0);out.location=(1450,0)
    def op(operation,a,b=0,label=''):
        n=ns.new('ShaderNodeMath');n.operation=operation;n.label=label or operation
        for s,v in zip(n.inputs,(a,b)):
            if hasattr(v,'is_output'):ls.new(v,s)
            else:s.default_value=v
        return n.outputs[0]
    phase=op('MULTIPLY',inp.outputs['相位'],inp.outputs['相位频率'], '① 相位贴图 × 频率')
    elapsed=op('MULTIPLY',inp.outputs['时间 秒'],inp.outputs['速度 rad/s'],'② 时间 × 速度')
    angle=op('ADD',phase,elapsed,'③ 相位随时间推进；UV 不动')
    wave=op('ADD',op('MULTIPLY',op('SINE',angle,label='④ 正弦波'),.5),.5,'⑤ 映射到 0–1')
    width=op('MAXIMUM',op('SUBTRACT',1,inp.outputs['阈值']),2**-24)
    gate=op('DIVIDE',op('SUBTRACT',wave,inp.outputs['阈值']),width,'⑥ 阈值 / 亮区宽度')
    gate=op('MINIMUM',op('MAXIMUM',gate,0),1)
    gate=op('POWER',gate,inp.outputs['锐度'],'⑦ 锐度 / 亮区形状')
    squared=op('MULTIPLY',inp.outputs['区域蒙版'],inp.outputs['区域蒙版'],'⑧ 区域蒙版平方')
    moving=op('MULTIPLY',squared,gate,'⑨ 实际移动亮区')
    facing=op('MINIMUM',op('MAXIMUM',inp.outputs['视角点积'],.001),.999)
    rim=op('MINIMUM',op('MAXIMUM',op('SUBTRACT',1,inp.outputs['视角点积']),.001),.999)
    front=op('POWER',facing,op('MAXIMUM',op('SUBTRACT',16,inp.outputs['视角参数']),0))
    edge=op('POWER',rim,op('MAXIMUM',op('SUBTRACT',inp.outputs['视角参数'],16),0))
    weight=op('MULTIPLY',op('MULTIPLY',moving,op('MINIMUM',front,edge)),inp.outputs['强度'],'⑩ 视角 × 强度')
    color=ns.new('ShaderNodeMixRGB');color.blend_type='MULTIPLY';color.inputs[0].default_value=1;color.label='⑪ 亮区 × HDR 颜色'
    ls.new(weight,color.inputs[1]);ls.new(inp.outputs['颜色'],color.inputs[2])
    for source,name in [(color.outputs[0],'发光'),(moving,'移动亮区'),(inp.outputs['区域蒙版'],'区域蒙版'),(inp.outputs['相位'],'相位')]:ls.new(source,out.inputs[name])
    # A compact left-to-right dependency layout, one calculation per slot.
    from .layout import arrange_material
    arrange_material(type('TreeView',(object,),{'node_tree':tree,'__setitem__':lambda *a:None})())
    quiet_previews(tree)
    return tree


def collapse(tree,selected,name,role):
    """Clone this adapter-owned DAG partition without depending on UI context."""
    selected=set(selected)
    if not selected:return None
    links=list(tree.links)
    inbound=[l for l in links if l.from_node not in selected and l.to_node in selected]
    outbound=[l for l in links if l.from_node in selected and l.to_node not in selected]
    inner=bpy.data.node_groups.new(name,'ShaderNodeTree')
    inner['naraka_group_contract']=role
    copies={}
    for n in tree.nodes:
        if n not in selected:continue
        copy=inner.nodes.new(n.bl_idname);copies[n]=copy
        # Operations may change socket layouts; set these before defaults.
        for prop in n.bl_rna.properties:
            key=prop.identifier
            if prop.is_readonly or key in {'name','parent','location','dimensions','width','height','select'}:continue
            try:setattr(copy,key,getattr(n,key))
            except (AttributeError,TypeError,ValueError):pass
        for key in n.keys():
            try:copy[key]=n[key]
            except TypeError:pass
        copy.name=n.name;copy.label=n.label;copy.location=n.location
        for old,new in zip(n.inputs,copy.inputs):
            if hasattr(old,'default_value'):
                try:new.default_value=old.default_value
                except (TypeError,ValueError):pass
    if tree.animation_data:
        for curve in list(tree.animation_data.drivers):
            if not any(curve.data_path.startswith(n.path_from_id()+'.') for n in selected):continue
            try:target=inner.driver_add(curve.data_path,curve.array_index)
            except TypeError:target=inner.driver_add(curve.data_path)
            target.driver.type=curve.driver.type;target.driver.expression=curve.driver.expression
            for variable in curve.driver.variables:
                var=target.driver.variables.new();var.name=variable.name;var.type=variable.type
                for old,new in zip(variable.targets,var.targets):
                    for key in ('id_type','id','data_path','bone_target','transform_type','transform_space','rotation_mode'):
                        try:setattr(new,key,getattr(old,key))
                        except (AttributeError,TypeError,ValueError):pass
            tree.driver_remove(curve.data_path,curve.array_index)
    def socket_copy(socket,node,output):
        old=node.outputs if output else node.inputs
        new=copies[node].outputs if output else copies[node].inputs
        return new[list(old).index(socket)]
    for l in links:
        if l.from_node in selected and l.to_node in selected:
            inner.links.new(socket_copy(l.from_socket,l.from_node,True),socket_copy(l.to_socket,l.to_node,False))
    inp=inner.nodes.new('NodeGroupInput');out=inner.nodes.new('NodeGroupOutput')
    def socket_type(s):
        return {'VALUE':'NodeSocketFloat','VECTOR':'NodeSocketVector','RGBA':'NodeSocketColor','SHADER':'NodeSocketShader','INT':'NodeSocketInt','BOOLEAN':'NodeSocketBool'}[s.type]
    incoming={};outgoing={}
    def unique(base,used):
        label=base;i=2
        while label in used:label=base+' '+str(i);i+=1
        used.add(label);return label
    labels=set()
    for l in inbound:
        source=l.from_socket
        if source not in incoming:
            label=unique((l.from_node.get('naraka_binding','')+' '+source.name).strip() if l.from_node.type in {'TEX_IMAGE','SEPARATE_COLOR'} else l.from_node.label or source.name,labels)
            interface(inner,label,'INPUT',socket_type(source));incoming[source]=label
        inner.links.new(inp.outputs[incoming[source]],socket_copy(l.to_socket,l.to_node,False))
    labels=set()
    translations={'Base Color':'基础色','Normal':'法线','Metallic':'金属度','Roughness':'粗糙度','Specular Tint':'镜面色',
        'Anisotropic':'各向异性','Anisotropic Rotation':'各向异性旋转','Sheen Weight':'绒面','Subsurface Weight':'透光',
        'Alpha':'覆盖与剔除','Vector':'流光采样 UV'}
    for l in outbound:
        source=l.from_socket
        if source not in outgoing:
            label=unique(translations.get(l.to_socket.name,l.from_node.label or source.name),labels)
            interface(inner,label,'OUTPUT',socket_type(source));outgoing[source]=label
            inner.links.new(socket_copy(source,l.from_node,True),out.inputs[label])
    group=tree.nodes.new('ShaderNodeGroup');group.node_tree=inner;group.label=name;group['naraka_role']=role
    for source,label in incoming.items():tree.links.new(source,group.inputs[label])
    for l in outbound:tree.links.new(group.outputs[outgoing[l.from_socket]],l.to_socket)
    for n in selected:tree.nodes.remove(n)
    from .layout import arrange_material
    arrange_material(type('TreeView',(object,),{'node_tree':inner,'__setitem__':lambda *a:None})())
    quiet_previews(inner)
    return group


def organize(material):
    from .texture_layout import organize_visible
    organize_visible(material,collapse)
    quiet_previews(material.node_tree)


def arrange_root(material):
    if material.get('naraka_readable_layout',0)>=2:
        from .texture_layout import arrange_visible
        arrange_visible(material)
        return
    tree=material.node_tree
    # Only the readable root uses this diagram layout; inner groups keep DAGs.
    slots={
        'surface_stage':(0,0),'coverage_stage':(0,-1050),'time':(0,450),
        'flow_texture':(500,450),'flow_channels':(850,450),'view_stage':(500,-600),
        'flow_layer_0':(1150,450),'flow_layer_1':(1150,-450),
        'emission_stage':(1650,200),'principled':(2050,100),'output':(2450,100),
        'closure_stage':(2050,100),'retained_stage':(0,-1600),
    }
    for n in tree.nodes:
        n.parent=None
        role=n.get('naraka_role','')
        if role=='flow_layer':role='flow_layer_%d'%n['naraka_flow_layer']
        if n.type=='BSDF_PRINCIPLED':role='principled'
        elif n.type=='OUTPUT_MATERIAL':role='output'
        elif n.get('naraka_section')=='time':role='time'
        n.location=slots.get(role,(1650,-800));n.width=300 if n.type=='GROUP' else 230;n.select=False
        # The Principled node exposes only its actual connections.
        if n.type=='BSDF_PRINCIPLED':
            for s in n.inputs:s.hide=not s.is_linked
        if n.type=='GROUP':
            for s in n.outputs:s.hide=not s.is_linked
    material['naraka_node_layout']=2


PREVIEW_MODES=[('NORMAL','完整材质',''),('MASK_1','流光1 · R 区域蒙版',''),('PHASE_1','流光1 · G 相位图',''),
    ('WAVE_1','流光1 · 移动亮区',''),('COLOR_1','流光1 · 最终发光',''),('MASK_2','流光2 · B 区域蒙版',''),
    ('PHASE_2','流光2 · A 相位图',''),('WAVE_2','流光2 · 移动亮区',''),('COLOR_2','流光2 · 最终发光',''),
    ('EMISSION','全部发光','')]


def set_preview(material,mode):
    """Output one stage on the actual mesh, keeping the source coverage mask."""
    tree=material.node_tree
    output=next(n for n in tree.nodes if n.type=='OUTPUT_MATERIAL')
    if 'naraka_normal_output_node' not in material:
        l=output.inputs['Surface'].links[0]
        material['naraka_normal_output_node']=l.from_node.name
        material['naraka_normal_output_socket']=list(l.from_node.outputs).index(l.from_socket)
    if mode=='NORMAL':
        for n in list(tree.nodes):
            if n.get('naraka_role')=='debug_preview':tree.nodes.remove(n)
        tree.links.new(tree.nodes[material['naraka_normal_output_node']].outputs[material['naraka_normal_output_socket']],output.inputs['Surface'])
        material['naraka_preview']='NORMAL';return
    if mode=='EMISSION':
        bsdf=next((n for n in tree.nodes if n.type=='BSDF_PRINCIPLED'),None)
        if bsdf is None or not bsdf.inputs['Emission Color'].is_linked:raise ValueError('当前材质没有独立发光输出')
        source=bsdf.inputs['Emission Color'].links[0].from_socket
    else:
        kind,number=mode.rsplit('_',1)
        wave=next(n for n in tree.nodes if n.get('naraka_role')=='flow_layer' and n['naraka_flow_layer']==int(number)-1)
        source=wave.outputs[{'MASK':'区域蒙版','PHASE':'相位','WAVE':'移动亮区','COLOR':'发光'}[kind]]
    for n in list(tree.nodes):
        if n.get('naraka_role')=='debug_preview':tree.nodes.remove(n)
    source.hide=False
    def new(kind,label,location):
        node=tree.nodes.new(kind);node.label=label;node['naraka_role']='debug_preview';node.location=location
        return node
    emission=new('ShaderNodeEmission','观察：'+dict((a,b) for a,b,_ in PREVIEW_MODES)[mode],(2500,-700))
    tree.links.new(source,emission.inputs[0])
    bsdf=next((n for n in tree.nodes if n.type=='BSDF_PRINCIPLED'),None)
    if bsdf and bsdf.inputs['Alpha'].is_linked:
        transparent=new('ShaderNodeBsdfTransparent','保留正反面与覆盖',(2500,-1000))
        mix=new('ShaderNodeMixShader','观察结果',(2850,-700))
        tree.links.new(bsdf.inputs['Alpha'].links[0].from_socket,mix.inputs[0])
        tree.links.new(transparent.outputs[0],mix.inputs[1]);tree.links.new(emission.outputs[0],mix.inputs[2])
        source=mix.outputs[0]
    else:source=emission.outputs[0]
    tree.links.new(source,output.inputs['Surface'])
    material['naraka_preview']=mode
    quiet_previews(tree)
