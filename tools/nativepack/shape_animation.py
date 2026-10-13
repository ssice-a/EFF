"""Sampled shape-weight clips; Mesh deltas are never duplicated here."""
import math
import struct
from .type_tree import require
from .resource_file import encode_resource, decode_strings, encode_strings

KIND = 8
BLOCK = '@shape-animation'

def encode(owner, clip, tracks):
    strings = [owner, clip['id'], clip['label'], clip.get('key', ''), clip['speedVariable']]
    data = bytearray(encode_strings(strings))
    data.extend(struct.pack('<ddBBI', clip['duration'], clip['speed'], bool(clip['loop']), bool(clip['autoplay']), len(tracks)))
    for variable, samples in tracks:
        data.extend(encode_strings([variable]))
        data.extend(struct.pack('<I', len(samples)))
        data.extend(struct.pack('<%df' % len(samples), *samples))
    return encode_resource(KIND, '00'*16, {BLOCK: data})

def decode(resource):
    require(resource['kind']==KIND and set(resource['blocks'])=={BLOCK}, 'shape animation resource layout')
    data=resource['blocks'][BLOCK]; offset=0
    def strings():
        nonlocal offset
        require(offset+4<=len(data),'animation strings truncated')
        count,=struct.unpack_from('<I',data,offset); start=offset;offset+=4
        require(count<=(len(data)-offset)//2,'animation string count')
        for _ in range(count):
            require(offset+2<=len(data),'animation string truncated');n,=struct.unpack_from('<H',data,offset);offset+=2+n
            require(offset<=len(data),'animation string range')
        return decode_strings(data[start:offset])
    names=strings();require(len(names)==5,'animation header strings')
    require(offset+22<=len(data),'animation header truncated')
    duration,speed,loop,auto,count=struct.unpack_from('<ddBBI',data,offset);offset+=22
    require(math.isfinite(duration) and duration>0 and math.isfinite(speed) and speed>0 and loop<=1 and auto<=1,'animation playback domain')
    require(count>0 and count<=(len(data)-offset)//14,'animation track count')
    tracks={}
    for _ in range(count):
        variables=strings();require(len(variables)==1,'animation track binding')
        require(offset+4<=len(data),'animation samples truncated');n,=struct.unpack_from('<I',data,offset);offset+=4
        require(n>=2 and n<=(len(data)-offset)//4,'animation samples range')
        samples=struct.unpack_from('<%df'%n,data,offset);offset+=4*n
        require(all(math.isfinite(v) for v in samples) and variables[0] not in tracks,'animation samples nonfinite/duplicate')
        tracks[variables[0]]=samples
    require(offset==len(data),'animation trailing bytes')
    return dict(owner=names[0],id=names[1],label=names[2],key=names[3],speedVariable=names[4],duration=duration,speed=speed,loop=bool(loop),autoplay=bool(auto),tracks=tracks)
