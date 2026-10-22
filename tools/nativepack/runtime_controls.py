"""Portable numeric material clips and isolated native effect attachments."""
import math
import struct
from pathlib import Path
from .resource_file import encode_resource, encode_strings, decode_resource, identifier, resource_path
from .shape_animation import encode as shape_encode, decode as shape_decode
from .hashing import sha256_bytes
from .type_tree import require

MATERIAL = 9
EFFECT = 10

def native_archive(data):
    """Validate the emitted, uncompressed game envelope before publication."""
    if len(data) < 48: return False
    def be(p,n): return int.from_bytes(data[p:p+n], 'big')
    def rol(v,n,bits): return ((v << n) | (v >> (bits-n))) & ((1 << bits)-1)
    a=be(0,4)^0x4A92F0CD
    if be(4,4)!=(((4*a)&0xFFFF0000)^rol(a,18,32)^0xD8B1E637):return False
    f2=be(10,4)
    if be(14,4)^f2!=8 or be(22,4)^f2^0xA7F49310!=0x240:return False
    low=be(18,4);packed=((be(34,4)^low^0xDAD76848)<<32)|low
    length=rol(packed,14,64)^0xA4F1A11747816520
    def size32(high,low):
        b=be(low,2);return rol((((be(high,2)^b^0xA121)&65535)<<16)|b,14,32)^0xF74324EE
    info,raw=size32(38,8),size32(26,32)
    return length==len(data) and info>0 and info==raw and ((info+15)&~15)<len(data)-48

class Reader:
    def __init__(self, data): self.data, self.offset = data, 0
    def raw(self, count):
        require(0 <= count <= len(self.data)-self.offset, 'runtime control truncated')
        value=self.data[self.offset:self.offset+count];self.offset+=count;return value
    def n(self, size=4): return int.from_bytes(self.raw(size), 'little')
    def strings(self):
        count=self.n();require(count <= (len(self.data)-self.offset)//2, 'runtime strings count')
        values=[self.raw(self.n(2)).decode('utf-8') for _ in range(count)]
        require(all('\0' not in s for s in values), 'runtime string NUL');return values
    def text64(self): return self.raw(self.n(8)).decode('utf-8')
    def end(self): require(self.offset==len(self.data), 'runtime control trailing bytes')

def material_encode(owner, clip):
    tracks=clip['targets'];data=bytearray(struct.pack('<I',len(tracks)))
    for row in tracks:
        data.extend(encode_strings([row['variable'],row['material'],row['property'],row['kind']]))
        data.extend(struct.pack('<B',row['component']))
    sampled=decode_resource(shape_encode(owner,clip,[(t['variable'],t['samples']) for t in tracks]),8)
    data.extend(sampled['blocks']['@shape-animation'])
    return encode_resource(MATERIAL,'00'*16,{'@material-animation':data})

def material_decode(resource):
    require(resource['kind']==MATERIAL and set(resource['blocks'])=={'@material-animation'},'material animation layout')
    r=Reader(resource['blocks']['@material-animation']);count=r.n();require(0<count<=(len(r.data)-r.offset)//15,'material animation targets')
    targets=[];bindings=set()
    for _ in range(count):
        names=r.strings();require(len(names)==4,'material track identity');variable,material,prop,kind=names;component=r.n(1)
        require(kind in ('FLOAT','COLOR','VECTOR') and component<(1 if kind=='FLOAT' else 4) and prop.startswith('_') and '\n' not in prop and '\t' not in prop,'material track type/property')
        require((material,prop,component) not in bindings,'duplicate material property channel');bindings.add((material,prop,component));identifier(material)
        targets.append(dict(variable=variable,material=material,property=prop,kind=kind,component=component))
    clip=shape_decode(dict(kind=8,blocks={'@shape-animation':r.data[r.offset:]}))
    require(len({t['variable'] for t in targets})==len(targets) and {t['variable'] for t in targets}==clip['tracks'].keys(),'material animation track coverage')
    clip['targets']=targets;return clip

def native_build(path):
    path=Path(path).resolve();path=path/'runtime.bin' if path.is_dir() else path
    r=Reader(path.read_bytes());require(r.text64()=='EFF_EFFECT_DEBUG_NATIVE/inactive-root+le64','native effect build structure')
    key,prefab,root_name=r.text64(),r.text64(),r.text64();root,particles,count=r.n(8),r.n(8),r.n(8)
    require(0<count<=(len(r.data)-r.offset)//24 and root<count,'effect bundle count/root')
    bundles=[]
    for _ in range(count):
        file,hash_value=r.text64(),r.text64();n=r.n(8);require(n<=count,'effect dependency count');deps=[r.n(8) for _ in range(n)]
        require(len(set(deps))==len(deps) and all(d<count and d!=len(bundles) for d in deps),'effect dependency graph')
        data=resource_path(path.parent,file).read_bytes()
        require(sha256_bytes(data).hex().upper()==hash_value,'effect build digest')
        require(native_archive(data),'特效资源包不是游戏原生封包，请使用更新后的 AnimeStudio 重新构建特效')
        bundles.append(dict(file=file,hash=hash_value,dependencies=deps,data=data))
    r.end();return dict(key=key,prefab=prefab,rootName=root_name,root=root,particles=particles,bundles=bundles)

def effect_encode(owner, effect):
    native=native_build(effect['build']);data=bytearray(encode_strings([owner,effect['id'],effect['label'],effect.get('key',''),effect['part'],effect.get('bone','')]))
    data.extend(struct.pack('<B',bool(effect.get('autoplay',True))))
    transform=list(effect.get('position',[0,0,0]))+list(effect.get('rotation',[0,0,0,1]))+list(effect.get('scale',[1,1,1]))
    require(len(transform)==10 and all(math.isfinite(v) and abs(v)<=3.402823466e38 for v in transform),'effect attachment transform')
    data.extend(struct.pack('<10f',*transform))
    data.extend(encode_strings([native['key'],native['prefab'],native['rootName']]))
    data.extend(struct.pack('<III',native['root'],native['particles'],len(native['bundles'])))
    for b in native['bundles']:
        data.extend(encode_strings([b['file'],b['hash']]))
        data.extend(struct.pack('<I',len(b['dependencies'])))
        for d in b['dependencies']:data.extend(struct.pack('<I',d))
        data.extend(struct.pack('<Q',len(b['data'])));data.extend(b['data'])
    blocks={'@effect-attachment':data}
    if effect.get('targetPrefab'):
        require(effect.get('rendererPath') and effect.get('targetPath') and effect.get('targetAsset') and effect.get('bones'),'effect source attachment proof missing')
        blocks['@effect-target']=encode_strings([effect['targetPrefab'],effect['rendererPath'],effect['targetPath'],effect['targetAsset']]+list(effect['bones']))
    return encode_resource(EFFECT,'00'*16,blocks)

def effect_decode(resource):
    require(resource['kind']==EFFECT and '@effect-attachment' in resource['blocks'] and set(resource['blocks'])<={'@effect-attachment','@effect-target'},'effect attachment layout')
    r=Reader(resource['blocks']['@effect-attachment']);names=r.strings();require(len(names)==6,'effect attachment identity')
    owner,cid,label,key,part,bone=names;identifier(owner);identifier(cid)
    if part:identifier(part)
    require(part or '@effect-target' in resource['blocks'],'effect has no target')
    autoplay=r.n(1);require(autoplay<=1,'effect autoplay domain');transform=struct.unpack('<10f',r.raw(40));require(all(math.isfinite(v) for v in transform) and abs(sum(v*v for v in transform[3:7])-1)<1e-3,'effect transform nonfinite/rotation');native=r.strings();require(len(native)==3,'effect native identity')
    root,particles,count=r.n(),r.n(),r.n();require(0<count<=(len(r.data)-r.offset)//16 and root<count,'effect native count/root')
    require(len(native[0])==64 and all(c in '0123456789ABCDEF' for c in native[0]) and native[1].startswith('assets/') and '..' not in native[1] and native[2],'effect native identity')
    for i in range(count):
        bundle=r.strings();require(len(bundle)==2,'effect bundle identity');n=r.n();require(n<=count,'effect dependency count');deps=[r.n() for _ in range(n)]
        require(len(set(deps))==len(deps) and all(d<count and d!=i for d in deps),'effect dependencies')
        data=r.raw(r.n(8));require(sha256_bytes(data).hex().upper()==bundle[1],'effect bundle SHA256')
        require(native_archive(data) or data.startswith(b'UnityFS\0'),'effect archive header')
    r.end();require(not bone.startswith('/') and not bone.endswith('/') and all(x not in ('','.','..') for x in bone.split('/')) if bone else True,'effect bone path')
    result=dict(owner=owner,id=cid,label=label,key=key,part=part,bone=bone,autoplay=bool(autoplay),variable='$fx_'+cid,position=list(transform[:3]),rotation=list(transform[3:7]),scale=list(transform[7:]))
    if '@effect-target' in resource['blocks']:
        from .resource_file import decode_strings
        target=decode_strings(resource['blocks']['@effect-target']);require(len(target)>4 and target[0].startswith('assets/') and target[0].endswith('.prefab'),'invalid effect target proof')
        result.update(targetPrefab=target[0],rendererPath=target[1],targetPath=target[2],targetAsset=target[3],bones=target[4:])
    return result

def publish_effect_mod(destination,label,effects):
    from .author_source import source_read_lock
    destination=Path(destination).resolve();destination.parent.mkdir(parents=True,exist_ok=True)
    with source_read_lock(destination.parent):
        return _publish_effect_mod(destination,label,effects)

def _publish_effect_mod(destination,label,effects):
    """Publish a self-contained effect Mod; no character compiler or live pipe."""
    import configparser,hashlib,os,shutil,tempfile
    destination=Path(destination).resolve();destination.parent.mkdir(parents=True,exist_ok=True)
    require(effects and all(e.get('targetPrefab') for e in effects),'standalone effect requires exact source model proof')
    existing=destination.exists() and any(destination.iterdir())
    if existing:
        prior=destination/'mod.ini'
        require(prior.is_file(),'destination is not an effect Mod')
        check=configparser.ConfigParser(interpolation=None);check.read(prior,encoding='utf-8-sig')
        require(check.get('Mod','type',fallback='')=='effect','refusing to replace a character Mod')
    staging=Path(tempfile.mkdtemp(prefix='.'+destination.name+'-effect-',dir=destination.parent))
    backup=destination.with_name('.'+destination.name+'-previous-effect')
    try:
        ini=configparser.ConfigParser(interpolation=None);ini.optionxform=str
        ident='Effect_'+hashlib.sha256(destination.name.encode()).hexdigest()[:16]
        ini['Mod']={'format':'2','id':ident,'type':'effect','key_switch_enabled':'1'};ini['Constants']={}
        if existing:
            ini['Mod']['id']=check.get('Mod','id',fallback=ident)
            ini['Mod']['key_switch_enabled']=check.get('Mod','key_switch_enabled',fallback='1')
            state=destination/'state.ini'
            if state.is_file():shutil.copy2(state,staging/'state.ini')
            from .mod_directory import child_mods
            for child in child_mods(destination):
                target=staging/child.relative_to(destination)
                target.parent.mkdir(parents=True,exist_ok=True)
                shutil.copytree(child,target)
        apply(staging,ini,effects=effects)
        # read_package is the same strict author decoder used for regular Mods.
        from .ini_reader import read_package
        read_package(staging)
        if backup.exists():shutil.rmtree(backup)
        if destination.exists():os.replace(destination,backup)
        try:os.replace(staging,destination)
        except BaseException:
            if backup.exists():os.replace(backup,destination)
            raise
        if backup.exists():shutil.rmtree(backup)
    finally:
        if staging.exists():shutil.rmtree(staging)

def apply(root, ini, material_clips=None, effects=None, material_ids=(), merge_effects=False):
    """Modify only the supplied domain; unrelated partial exports preserve it."""
    from .ini_package import save_ini
    from .resource_file import read_resource
    root=Path(root)
    for prefix,rows,kind,decode in (('MaterialAnimation',material_clips,MATERIAL,material_decode),('EffectAttachment',effects,EFFECT,effect_decode)):
        if rows is None:continue
        for section in list(ini.sections()):
            if not section.startswith(prefix):continue
            prior=decode(read_resource(resource_path(root,ini[section]['path']),kind))
            if kind==EFFECT and merge_effects and prior['id'] not in {row['id'] for row in rows}:continue
            if kind==MATERIAL and material_ids and not any(t['material'] in material_ids for t in prior['targets']):continue
            if kind==MATERIAL and material_ids:require(all(t['material'] in material_ids for t in prior['targets']),'partial export must include every material in the animation')
            variables=list(prior['tracks'])+[prior['speedVariable']] if kind==MATERIAL else [prior['variable']]
            for variable in variables:
                for key in (variable,'persist '+variable):ini['Constants'].pop(key,None)
                for control in list(ini.sections()):
                    if control.startswith(('ShapeControl','KeyRuntime')) and (ini[control].get('variable')==variable or variable in ini[control]):ini.remove_section(control)
            resource_path(root,ini[section]['path']).unlink();ini.remove_section(section)
        for row in rows:
            cid=identifier(row['id']);relative='resources/'+prefix.lower()+'_'+cid+'.bin';path=resource_path(root,relative);path.parent.mkdir(parents=True,exist_ok=True)
            if kind==MATERIAL:
                row=dict(row,speedVariable='$mat_speed_'+cid)
                data=material_encode(ini['Mod']['id'],row);decoded=material_decode(decode_resource(data,kind))
                ini['Constants']['persist '+row['speedVariable']]=str(row['speed'])
                for target in row['targets']:
                    variable=target['variable'];values=decoded['tracks'][variable];low,high=min(values),max(values)
                    if low==high:high=low+max(1,abs(low)*.01)
                    ini['Constants'][variable]=str(values[0]);ini['ShapeControlRuntime'+variable[1:]]=dict(variable=variable,label=row['label']+' '+target['property'],min=str(low),max=str(high))
            else:
                data=effect_encode(ini['Mod']['id'],row);variable='$fx_'+cid;ini['Constants']['persist '+variable]=str(int(row.get('autoplay',True)))
                ini['ShapeControlRuntime'+variable[1:]]=dict(variable=variable,label=row['label'],min='0',max='1')
                if row.get('key'):ini['KeyRuntime'+cid]={'key':row['key'],'type':'cycle',variable:'0,1'}
            temp=path.with_suffix('.tmp');temp.write_bytes(data);temp.replace(path);ini[prefix+cid]={'path':relative}
    save_ini(root,ini)
