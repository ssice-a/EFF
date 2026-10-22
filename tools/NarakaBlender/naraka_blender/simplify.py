"""Explicit migration of existing materials, without reimporting geometry."""
import json
from datetime import datetime
from pathlib import Path

import bpy

from .format import load_package
from .materials import MaterialBuilder


def material_backup_path(backup_directory=None):
    if backup_directory:
        root=Path(backup_directory)
    elif bpy.data.filepath:
        root=Path(bpy.data.filepath).parent / 'NarakaMaterialBackups'
    else:
        root=Path(bpy.app.tempdir) / 'NarakaMaterialBackups'
    root.mkdir(parents=True,exist_ok=True)
    return root / ('naraka-materials-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f') + '.blend')


def cleanup_unused_legacy(backup_directory=None):
    """Move unreferenced old importer materials out of the author's blend file."""
    old={m for m in bpy.data.materials if m.get('naraka_parameters_json')
         and m.get('naraka_material_profile') != 'LIGHTWEIGHT'
         and m.users == int(m.use_fake_user) and m.asset_data is None}
    if not old:
        return {'materials':0}
    backup=material_backup_path(backup_directory)
    bpy.data.libraries.write(str(backup),old,fake_user=True,compress=True)
    images=set()
    def collect(tree,seen):
        if tree in seen:return
        seen.add(tree)
        for n in tree.nodes:
            if n.type=='TEX_IMAGE' and n.image:images.add(n.image)
            elif n.type=='GROUP':collect(n.node_tree,seen)
    for m in old:collect(m.node_tree,set())
    count=len(old)
    for m in old:bpy.data.materials.remove(m)
    removed=True
    while removed:
        removed=False
        for tree in list(bpy.data.node_groups):
            if tree.get('naraka_group_contract') and tree.users == 0:
                bpy.data.node_groups.remove(tree);removed=True
    image_count=0
    for image in images:
        if image.users==0 and image.get('naraka_source'):
            bpy.data.images.remove(image);image_count+=1
    return {'materials':count,'images':image_count,'backup':str(backup)}


def edited_source(material):
    source = json.loads(material["naraka_parameters_json"])
    source['name'] = material.name
    # Preserve the two exposed flow layers' current timing and color edits.
    waves = [n for n in material.node_tree.nodes if n.get('naraka_role') == 'flow_layer']
    for node in waves:
        i = node['naraka_flow_layer']
        keys = ('相位频率','速度 rad/s','阈值','锐度')
        source['vectors']['_MatEffectParam%d' % i] = [node.inputs[k].default_value for k in keys]
        tint = [v * node.inputs['强度'].default_value for v in node.inputs['颜色'].default_value[:3]] + [1]
        source['vectors']['_EmissionColor' if i == 0 else '_MatEffectColor0'] = tint
    if waves:
        extra = source['vectors'].setdefault('_MatEffectParam2',[1,0,16,16])
        extra[1] = 0
    return source


def package_for(material, cache):
    roots = []
    for col in bpy.data.collections:
        if col.get('naraka_package'):
            roots.append(Path(col['naraka_package']))
    for node in material.node_tree.nodes:
        if node.type == 'TEX_IMAGE' and node.image and node.image.filepath:
            roots.extend(Path(bpy.path.abspath(node.image.filepath)).parents)
    seen = set()
    for root in roots:
        if root in seen:
            continue
        seen.add(root)
        if not (root / 'naraka.json').is_file():
            continue
        if root not in cache:
            cache[root] = load_package(root / 'naraka.json')
        root, manifest = cache[root]
        if material['naraka_source'] in manifest['materials']:
            return root, manifest
    raise ValueError('找不到材质的原始 .naraka 包：' + material.name)


def simplify_materials(objects, backup_directory=None):
    objects = list(objects)
    originals = {slot.material for obj in objects for slot in getattr(obj,'material_slots',[])
                 if slot.material and slot.material.get('naraka_parameters_json')
                 and slot.material.get('naraka_material_profile') != 'LIGHTWEIGHT'}
    if not originals:
        return {'materials':0}
    # Resolve every source before any change to existing material slots.
    packages = {}
    sources = {m: (package_for(m,packages),edited_source(m)) for m in originals}
    backup = material_backup_path(backup_directory)
    bpy.data.libraries.write(str(backup), originals, fake_user=True, compress=True)
    before = {kind:set(getattr(bpy.data,kind)) for kind in ('materials','node_groups','images')}
    replacements, builders = {}, {}
    try:
        for old, ((root,manifest),source) in sorted(sources.items(),key=lambda p:p[0].name):
            if root not in builders:
                builders[root] = MaterialBuilder(root,manifest,[])
                # Migration may reuse matching source images without changing them.
                for image in bpy.data.images:
                    identity=image.get('naraka_source')
                    if identity not in manifest['textures']:
                        continue
                    expected=(root / manifest['textures'][identity]['path']).resolve()
                    if image.filepath and Path(bpy.path.abspath(image.filepath)).resolve()==expected:
                        builders[root].images[(identity,image.colorspace_settings.name=='Non-Color',None)]=image
            replacements[old]=builders[root].build(source)
    except Exception:
        for kind in ('materials','node_groups','images'):
            for block in set(getattr(bpy.data,kind))-before[kind]:
                getattr(bpy.data,kind).remove(block,do_unlink=True)
        raise
    slots = [(slot,slot.material) for obj in objects for slot in getattr(obj,'material_slots',[])
             if slot.material in replacements]
    try:
        for slot,old in slots:
            slot.material=replacements[old]
    except Exception:
        for slot,old in slots:
            slot.material=old
        raise
    counts = []
    for old,new in replacements.items():
        name=old.name
        counts.append({'name':name,'oldRootNodes':len(old.node_tree.nodes),'newRootNodes':len(new.node_tree.nodes)})
        new['naraka_previous_material_backup']=str(backup)
        if old.users == int(old.use_fake_user):
            bpy.data.materials.remove(old)
            new.name=name
    # Only remove orphaned computation trees owned by this importer.
    removed=True
    while removed:
        removed=False
        for tree in list(bpy.data.node_groups):
            if tree.get('naraka_group_contract') and tree.users == 0:
                bpy.data.node_groups.remove(tree);removed=True
    return {'materials':len(replacements),'slots':len(slots),'backup':str(backup),'nodes':counts}
