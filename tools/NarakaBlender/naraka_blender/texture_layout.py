"""Visible source images/channels; only computation may enter node groups."""
import json

from .advanced_cloth import VERIFIED_SHADERS
from .emission import STANDARD_PHASE_SHADERS, phase_cloth_shader, periodic2_enabled


def contract(source, binding, slice_index=None):
    shader=source['shader'].lower().replace('\\','/').rsplit('/',1)[-1]
    cloth=phase_cloth_shader(shader)
    skin=shader=='lx22standardcharacterskin.shader'
    title='原始绑定（用途待核对）'
    channels=['R · 原始数据','G · 原始数据','B · 原始数据','A · 原始数据']
    uv='UV1 · 第二套' if binding.endswith('UV2') else 'UV0 · 第一套'
    if binding in ('_MainTex','_MainTexUV2','_BaseMap','_MainTexture'):
        title='基础色 / sRGB' if binding!='_MainTexUV2' else '第二套基础色 / sRGB'
        channels=['R · 底色','G · 底色','B · 底色','A · 源覆盖数据']
        if binding=='_MainTexUV2':channels[3]='A · UV1叠加权重（非透明）'
    if binding in ('_BumpMap','_BumpMapUV2','_NormalMap','_NormalBentMap'):
        title='切线法线 / Non-Color'
        channels=['R · 编码X','G · 编码Y','B · 不直接用作Z','A · 格式相关数据']
        if binding=='_BumpMapUV2':channels[3]='A · 追加细节蒙版'
        elif binding=='_NormalBentMap':channels[2:] = ['B · bent-normal数据','A · bent-normal数据']
    if binding in ('_SpecGlossMap','_SpecGlossMapUV2'):
        if cloth:
            title='MRAV / Non-Color'
            channels=['R · 金属度','G · 粗糙度','B · AO（游戏光照项）','A · 此适配未使用']
        elif skin:
            title='皮肤表面数据 / Non-Color'
            channels=['R · 光滑度','G · AO（游戏光照项）','B · 厚度（游戏SSS项）','A · 此适配未使用']
    if binding=='_AdvIDTex' and shader in VERIFIED_SHADERS:
        title='ID分区 / Non-Color'
        channels=['R · 绒面区域','G · 各向异性区域','B · 透光区域','A · 虹彩（未完整复刻）']
    if binding=='_ClothFeatureTexture':
        title='布料镜面分区 / Non-Color'
        channels=['R · 分区R','G · 分区G','B · 分区B','A · 分区A']
    if binding=='_TillingChannelMask' and shader in VERIFIED_SHADERS:
        title='四层细节覆盖 / Non-Color'
        channels=['R · 光学层1','G · 光学层2','B · 细节层3','A · 细节层4']
    if binding=='_EmissionMap' and periodic2_enabled(shader, source.get('floats',{}), source.get('keywords','').split()):
        title='流光数据 / Non-Color'
        channels=['R · 流光1区域','G · 流光1相位','B · 流光2区域','A · 流光2相位']
        selector=int(source.get('floats',{}).get('_EffectUvSelector',1))
        if shader in VERIFIED_SHADERS:
            chosen=['UV%d'%i for i in range(3) if selector & (1<<i)]
            uv='+'.join(chosen) if chosen else '零坐标'
            uv+=' · 固定采样，相位推进'
        elif shader in STANDARD_PHASE_SHADERS:uv+=' · _MainTex缩放/偏移 · 固定采样，相位推进'
        else:uv+=' · 固定采样，相位推进'
    elif binding=='_EmissionMap' and cloth:
        title='普通发光 / 贴图原色彩空间'
        channels=['R · 发光颜色R','G · 发光颜色G','B · 发光颜色B','A · 不用于表面透明度']
    if binding=='_DetailTillingArray':
        i=int(slice_index or 0)
        keywords=set(source.get('keywords','').split())
        if i<2:
            feature='闪片' if '_MASKED_GLITTER%d'%(i+1) in keywords else '钻石' if '_MASKED_DIAMONDS%d'%(i+1) in keywords else '光学层'
            title='数组 slice %d · %s'%(i,feature)
            channels=(['R · 微法线X','G · 微法线Y','B · 闪烁相位','A · 效果调制'] if feature=='闪片' else
                      ['R · 光学数据','G · 光学数据','B · 光学数据','A · 折射后取样（非透明）'])
        else:
            title='数组 slice %d · 细节层%d'%(i,i+1)
            channels=['R · 细节法线X','G · 细节法线Y','B · 细节权重','A · 此细节分支未使用']
        st=source.get('vectors',{}).get('_DetailTilling_ST%d'%(i+1),[1,1,0,0])
        uv='UV0 · 平铺(%g,%g) 偏移(%g,%g)'%tuple(st)
    if shader=='fx_uber_transparent.shadergraph':
        roles={'_MainTexture':'第一层滚动噪声','_SecondTexture':'第二层滚动噪声','_MaskTexture':'效果轮廓蒙版','_DistortTexture':'UV扭曲数据'}
        if binding in roles:
            title=roles[binding]
            selection=source.get('vectors',{}).get('_TextureUVSelection',[0]*4)
            index=('_MainTexture','_SecondTexture','_MaskTexture','_DistortTexture').index(binding)
            uv='UV%d'%(1 if selection[index]>=.5 else 0)
            move=source.get('vectors',{}).get({'_MainTexture':'_MainTextureAnimateRotate','_SecondTexture':'_SecondTextureScrollRotate','_MaskTexture':'_MaskTextureScrollRotate','_DistortTexture':'_DistortScrollAnchor'}[binding],[0]*4)
            uv+=' · 滚动(%g,%g)/秒'%(move[0],move[1])
            if binding=='_DistortTexture':channels=['R · 扭曲X编码','G · 扭曲Y编码','B · 当前分支未使用','A · 法线编码参与X']
            else:channels=['R · 图层颜色','G · 图层颜色','B · 图层颜色','A · 图层覆盖/预乘']
    return title,channels,uv


def expose(tree,source):
    """Annotate real image output sockets and directly linked channel splits."""
    images=[n for n in tree.nodes if n.type=='TEX_IMAGE']
    links=list(tree.links)
    for image in images:
        binding=image.get('naraka_binding',image.label)
        title,channels,uv=contract(source,binding,image.get('naraka_array_slice'))
        if 'refracted' in image.label.lower():uv+=' · 折射坐标再取样'
        image['naraka_texture_usage']=title+'；'+'；'.join(channels)
        image['naraka_uv_usage']=uv
        image['naraka_keep_root']=True
        image['naraka_role']='texture_source'
        image.label=('流光数据 '+binding) if binding=='_EmissionMap' else binding+' · '+title
        image.outputs[0].name='RGB · '+title.split('/')[0].strip()
        image.outputs[1].name=channels[3]
        for l in links:
            if l.from_node==image and l.to_node.type=='SEPARATE_COLOR':
                split=l.to_node
                split['naraka_binding']=binding
                split['naraka_texture_source']=image.name
                split['naraka_keep_root']=True
                split['naraka_role']='texture_channels'
                # Built-in socket labels stay Red/Green/Blue in Blender's UI.
                # Keep the semantic mapping visible in the node header as well.
                split.label=' | '.join(channels[:3])
                for socket,label in zip(split.outputs,channels[:3]):socket.name=label
    # One explicit node for each actual UV set, shared by its readers.
    maps={}
    for n in list(tree.nodes):
        if n.type!='UVMAP':continue
        key=(n.uv_map,n.from_instancer)
        if key in maps:
            for l in list(tree.links):
                if l.from_node==n:tree.links.new(maps[key].outputs[0],l.to_socket)
            tree.nodes.remove(n)
        else:
            maps[key]=n;n.label=n.uv_map+' · '+{'UV0':'第一套 UV','UV1':'第二套 UV','UV2':'第三套 UV'}.get(n.uv_map,'源 UV')


def organize_visible(material,collapse):
    tree=material.node_tree
    source=json.loads(material['naraka_parameters_json'])
    expose(tree,source)
    exposed={n for n in tree.nodes if n.type in {'TEX_IMAGE','UVMAP','MAPPING','GROUP','BSDF_PRINCIPLED','OUTPUT_MATERIAL'} or n.get('naraka_keep_root') or n.get('naraka_section')=='time'}
    nodes=[n for n in tree.nodes if n.type!='FRAME']
    incoming={n:[] for n in nodes}
    for link in tree.links:incoming[link.to_node].append(link.from_node)
    ranks={};visiting=set()
    def rank(n):
        if n in ranks:return ranks[n]
        if n in visiting:raise ValueError('Unexpected material dependency cycle')
        visiting.add(n)
        ranks[n]=max((rank(p)+(1 if p in exposed else 0) for p in incoming[n]),default=0)
        visiting.remove(n);return ranks[n]
    for n in nodes:rank(n)
    # A boundary is crossed at every visible source/split/group. Grouping by
    # this rank cannot create texture -> group -> texture feedback cycles.
    buckets={}
    for n in nodes:
        if n not in exposed:buckets.setdefault((ranks[n],n.get('naraka_section','运算')),[]).append(n)
        else:n['naraka_boundary_rank']=ranks[n]
    # Merge only partitions that would form a dependency cycle. Ordinary
    # sections stay separate, with a small set of purpose-named sockets.
    membership={n:key for key,members in buckets.items() for n in members}
    adjacency={key:set() for key in buckets}
    for link in tree.links:
        a,b=membership.get(link.from_node),membership.get(link.to_node)
        if a is not None and b is not None and a!=b:adjacency[a].add(b)
    indices={};low={};stack=[];on_stack=set();components=[]
    def visit(key):
        indices[key]=low[key]=len(indices);stack.append(key);on_stack.add(key)
        for child in adjacency[key]:
            if child not in indices:visit(child);low[key]=min(low[key],low[child])
            elif child in on_stack:low[key]=min(low[key],indices[child])
        if low[key]==indices[key]:
            group=[]
            while True:
                child=stack.pop();on_stack.remove(child);group.append(child)
                if child==key:break
            components.append(group)
    for key in buckets:
        if key not in indices:visit(key)
    names={'detail':'法线与细节','cloth':'ID分区','uv2':'UV1覆盖','surface':'表面输出','normal':'法线解码',
        'emission':'视角','emission_mix':'发光合成','alpha':'正反面/覆盖','effect':'特效图层','glitter':'闪片','diamond':'钻石','optical':'光学采样','base':'基础色'}
    for component in sorted(components,key=lambda group:min(group)):
        r=min(k[0] for k in component)
        members=[n for key in component for n in buckets[key]]
        purposes=list(dict.fromkeys(names.get(n.get('naraka_section'),n.get('naraka_section','运算')) for n in members))
        title=' / '.join(purposes[:3])+' · 阶段%d'%r
        group=collapse(tree,members,title,'computation_stage')
        group['naraka_boundary_rank']=r;group['naraka_stage_purposes']=' / '.join(purposes)
    for n in list(tree.nodes):
        if n.type=='FRAME':tree.nodes.remove(n)
    material['naraka_readable_layout']=2
    arrange_visible(material)


def arrange_visible(material):
    from .layout import node_size
    tree=material.node_tree
    for n in tree.nodes:
        if n.parent and n.parent.get('naraka_texture_frame'):n.parent=None
    for n in list(tree.nodes):
        if n.type=='FRAME' and n.get('naraka_texture_frame'):tree.nodes.remove(n)
    images=sorted([n for n in tree.nodes if n.type=='TEX_IMAGE'],key=lambda n:(n.get('naraka_binding',''),n.get('naraka_array_slice',-1),n.name))
    row_y=650
    rows={}
    for i,image in enumerate(images):
        rows.setdefault(i//3,[]).append(image)
    for row,members in rows.items():
        row_height=max(max(350,len([n for n in tree.nodes if n.get('naraka_texture_source')==im.name])*260)+180 for im in members)
        for col,image in enumerate(members):
            frame=tree.nodes.new('NodeFrame');frame['naraka_texture_frame']=True
            frame.label=image.get('naraka_binding','源贴图')+' | '+image.get('naraka_uv_usage','源UV')
            frame.label_size=24;frame.shrink=True
            frame.location=(400+col*1000,row_y)
            image.parent=frame;image.location=(30,-70);image.width=320;image.select=False
            splits=[n for n in tree.nodes if n.get('naraka_texture_source')==image.name]
            for j,split in enumerate(splits):
                split.parent=frame;split.location=(420,-70-j*260);split.width=340;split.select=False
        row_y-=row_height+160
    maps=[n for n in tree.nodes if n.type in {'UVMAP','MAPPING'}]
    for i,n in enumerate(maps):
        n.location=(-100,650-i*550);n.width=280;n.select=False
    columns={}
    for n in tree.nodes:
        if n.type=='FRAME' or n.parent or n in maps:continue
        n.select=False;n.width=380 if n.type=='GROUP' else 280
        r=int(n.get('naraka_boundary_rank',0))
        if n.type=='BSDF_PRINCIPLED':
            for s in n.inputs:s.hide=not s.is_linked
        elif n.get('naraka_section')=='time':n.location=(-100,1150);continue
        if n.type=='GROUP':
            for s in n.outputs:s.hide=not s.is_linked
        columns.setdefault(r,[]).append(n)
    # Stack by measured socket count; group interfaces must never overlap.
    for r,column in sorted(columns.items()):
        y=650
        for n in sorted(column,key=lambda n:(0 if n.get('naraka_role')=='flow_layer' else 1,n.label)):
            n.location=(3550+r*700,y)
            y-=node_size(n)[1]+180
    material['naraka_node_layout']=3
