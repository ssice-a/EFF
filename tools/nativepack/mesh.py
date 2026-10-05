"""Encode author arrays into the current Endfield Mesh TypeTree layout."""
from __future__ import annotations
import copy
import numpy as np
from .type_tree import require


def vector(values):
    return dict(zip(('x', 'y', 'z'), (float(v) for v in values)))


def aabb(positions):
    low, high = positions.min(axis=0), positions.max(axis=0)
    return dict(m_Center=vector((low+high)*.5), m_Extent=vector((high-low)*.5))


def unit(values, label):
    lengths = np.linalg.norm(values, axis=1)
    require(np.all(np.isfinite(lengths)) and np.all(lengths > 1e-10), 'degenerate '+label)
    return values / lengths[:, None]


def unpack_frames(words):
    def signed(shift):
        bits = ((words >> shift) & 1023).astype(np.int32)
        return np.where(bits >= 512, bits-1024, bits) / 511.
    x, y, angle = signed(0), signed(10), signed(20)
    z = 1-np.abs(x)-np.abs(y)
    old_x = x.copy()
    x = np.where(z < 0, (1-np.abs(y))*np.where(x >= 0, 1., -1.), x)
    y = np.where(z < 0, (1-np.abs(old_x))*np.where(y >= 0, 1., -1.), y)
    normal = unit(np.column_stack((x,y,z)), 'decoded normal')
    seed = np.column_stack((normal[:,1]-normal[:,2], normal[:,2]-normal[:,0], normal[:,0]-normal[:,1]))
    basis = unit(seed-np.sum(seed*normal, axis=1)[:,None]*normal, 'frame basis')
    bitangent = unit(np.cross(normal,basis), 'bitangent basis')
    dx = 1-2*np.abs(angle)
    dy = (1-np.abs(dx))*np.where(angle < 0, -1., 1.)
    tangent = unit(dx[:,None]*basis+dy[:,None]*bitangent, 'decoded tangent')
    handed = np.where(words >> 31, 1., -1.)
    return normal, np.column_stack((tangent,handed))


def expand_shaped_frames(mesh):
    """Give native CPU shape/skin jobs Float32 normal and tangent channels."""
    if not mesh['m_Shapes']['channels'].items():
        return False
    vertex = mesh['m_VertexData']
    count = vertex['m_VertexCount'].data
    channels = vertex['m_Channels'].items()
    require(0 < count <= 4000000 and len(channels) == 14, 'shaped vertex count/channels')
    descriptors = [{k: c[k].data for k in ('stream', 'offset', 'format', 'dimension')} for c in channels]
    widths = (4, 2, 1, 1, 2, 2, 1, 1, 2, 2, 4, 4)
    strides, starts = [0]*4, [0]*4
    occupied = [set() for _ in range(4)]
    for c in descriptors:
        dim = c['dimension'] & 15
        if not dim:
            continue
        s, off, fmt = c['stream'], c['offset'], c['format']
        require(0 < dim <= 4 and 0 <= s < 4 and 0 <= fmt < 12 and 0 <= off and off+dim*widths[fmt] <= 256,
                'shaped vertex channel extent')
        extent = set(range(off, off+dim*widths[fmt]))
        require(not occupied[s] & extent, 'shaped vertex overlapping channels')
        occupied[s] |= extent
        strides[s] = max(strides[s], off+dim*widths[fmt])
    size = 0
    for s in range(4):
        starts[s] = size
        size = (size+count*strides[s]+15) & ~15
    old = vertex['m_DataSize'].data
    require(len(old) == size, 'shaped vertex byte extent')
    require(descriptors[0] == dict(stream=0, offset=0, format=0, dimension=3), 'shaped position contract')
    if (descriptors[1] == dict(stream=0, offset=12, format=0, dimension=3) and
        descriptors[2] == dict(stream=0, offset=24, format=0, dimension=4) and strides[0] == 40):
        return False
    require(descriptors[1] == dict(stream=0, offset=12, format=0, dimension=0x31) and
            not descriptors[2]['dimension'] & 15 and strides[0] == 16, 'unsupported shaped frame storage')
    words = np.ndarray((count,), dtype='<u4', buffer=old, offset=12, strides=(16,)).copy()
    require(np.all(words & 0x40000000), 'shaped packed frame marker missing')
    normal, tangent = unpack_frames(words)
    stream = np.empty((count, 10), dtype='<f4')
    stream[:, :3] = np.ndarray((count, 3), dtype='<f4', buffer=old, strides=(16, 4))
    stream[:, 3:6], stream[:, 6:10] = normal, tangent
    data = bytearray(stream.tobytes())
    data.extend(bytes((-len(data)) % 16))
    for s in range(1, 4):
        data.extend(old[starts[s]:starts[s]+count*strides[s]])
        data.extend(bytes((-len(data)) % 16))
    channels[1].set('dimension', 3)
    for name, value in dict(stream=0, offset=24, format=0, dimension=4).items():
        channels[2].set(name, value)
    vertex.set('m_DataSize', bytes(data))
    return True


def pack_frames(normals, tangents):
    normals = unit(normals, 'author normal')
    octa = normals / np.sum(np.abs(normals), axis=1)[:,None]
    x, y = octa[:,0].copy(), octa[:,1].copy()
    x = np.where(octa[:,2]<0, (1-np.abs(octa[:,1]))*np.where(octa[:,0]>=0,1.,-1.), x)
    y = np.where(octa[:,2]<0, (1-np.abs(octa[:,0]))*np.where(octa[:,1]>=0,1.,-1.), y)
    qx, qy = np.rint(x*511).astype(np.int32), np.rint(y*511).astype(np.int32)
    words = ((qx & 1023) | ((qy & 1023)<<10)).astype(np.uint32) | np.uint32(0x40000000)
    quantized_normals, _ = unpack_frames(words)
    seed = np.column_stack((quantized_normals[:,1]-quantized_normals[:,2], quantized_normals[:,2]-quantized_normals[:,0], quantized_normals[:,0]-quantized_normals[:,1]))
    basis = unit(seed-np.sum(seed*quantized_normals,axis=1)[:,None]*quantized_normals, 'quantized basis')
    bitangent = unit(np.cross(quantized_normals,basis), 'quantized bitangent')
    target = unit(tangents[:,:3]-np.sum(tangents[:,:3]*quantized_normals,axis=1)[:,None]*quantized_normals, 'author tangent')
    dx, dy = np.sum(target*basis,axis=1), np.sum(target*bitangent,axis=1)
    dx /= np.abs(dx)+np.abs(dy)
    angle = (1-dx)*.5*np.where(dy<0,-1.,1.)
    qa = np.rint(angle*511).astype(np.int32)
    require(np.all(np.isclose(np.abs(tangents[:,3]),1,atol=1e-6)), 'invalid tangent handedness')
    words |= ((qa & 1023).astype(np.uint32)<<20) | ((tangents[:,3]>0).astype(np.uint32)<<31)
    decoded_n, decoded_t = unpack_frames(words)
    normal_error = float(np.max(np.abs(decoded_n-normals)))
    tangent_error = float(np.max(np.abs(decoded_t[:,:3]-unit(tangents[:,:3],'author tangent'))))
    require(normal_error <= .006 and tangent_error <= .015, f'packed-frame precision exceeded: normal={normal_error}, tangent={tangent_error}')
    return words.astype('<u4'), dict(normalMaxError=normal_error,tangentMaxError=tangent_error)


def native_mesh(template, author, name):
    mesh = copy.deepcopy(template)
    count = author['vertex_count']
    positions = np.asarray(author['vertices'],dtype=np.float64).reshape(count,3)
    require(count>0 and np.all(np.isfinite(positions)), 'empty or nonfinite author geometry')
    normals = np.asarray(author['normals'],dtype=np.float64).reshape(count,3)
    tangents = np.asarray(author['tangents'],dtype=np.float64).reshape(count,4)
    require(not author['additional'], 'additional native shape-normal payload is unsupported by the source schema')
    require(not mesh['m_CollisionMeshBaked'].data and not mesh['m_CollisionMeshOnly'].data, 'collision-only source Mesh unsupported')
    frames, precision = pack_frames(normals,tangents)
    source_channels = mesh['m_VertexData']['m_Channels'].items()
    require(len(source_channels)==14 and source_channels[1]['dimension'].data==0x31, 'source is not the proven packed-frame layout')
    channels = [dict(stream=0,offset=0,format=0,dimension=0) for _ in range(14)]
    stream0 = np.empty((count,4),dtype='<f4')
    stream0[:,:3] = positions
    stream0[:,3].view('<u4')[:] = frames
    channels[0] = dict(stream=0,offset=0,format=0,dimension=3)
    channels[1] = dict(stream=0,offset=12,format=0,dimension=0x31)
    arrays = []
    offset = 0
    for i,values in enumerate(author['uvs']):
        if not values: continue
        dim = len(values)//count
        require(2<=dim<=4 and len(values)==count*dim, 'UV shape')
        flags = source_channels[i+4]['dimension'].data & 0xf0
        channels[i+4] = dict(stream=1,offset=offset,format=0,dimension=flags|dim)
        arrays.append(np.asarray(values,dtype='<f4').reshape(count,dim))
        offset += dim*4
    if author['colors']:
        channels[3]=dict(stream=1,offset=offset,format=0,dimension=4)
        arrays.append(np.asarray(author['colors'],dtype='<f4').reshape(count,4))
        offset+=16
    require(offset<=255, 'vertex stream stride exceeds byte offset')
    stream1 = np.column_stack(arrays).astype('<f4').tobytes()
    weights = np.asarray([w for w,_ in author['skin']],dtype=np.float64)
    indices = np.asarray([b for _,b in author['skin']],dtype=np.uint32)
    require(indices.max()<256, 'this native UInt8 skin experiment requires fewer than 256 palette slots')
    # Existing native contract stores four normalized UNorm16 influences.
    weights /= weights.sum(axis=1)[:,None]
    scaled = weights*65535
    quantized = np.floor(scaled).astype(np.uint32)
    remainder = 65535-quantized.sum(axis=1)
    order = np.argsort(-(scaled-quantized),axis=1,kind='stable')
    for extra in range(4):
        rows=np.where(remainder>extra)[0]
        quantized[rows,order[rows,extra]]+=1
    require(np.all(quantized.sum(axis=1)==65535), 'UNorm16 weight sum')
    skin=np.empty(count,dtype=[('weights','<u2',(4,)),('bones','u1',(4,))])
    skin['weights']=quantized
    skin['bones']=indices
    channels[12]=dict(stream=2,offset=0,format=4,dimension=4)
    channels[13]=dict(stream=2,offset=8,format=6,dimension=4)
    vertex_data=bytearray()
    for stream in (stream0.tobytes(),stream1,skin.tobytes()):
        vertex_data.extend(stream)
        vertex_data.extend(bytes((-len(vertex_data))%16))
    mesh.set('m_Name',name)
    write_shapes(mesh,author)
    low_positions, high_positions = shape_envelope(author)
    local_bounds=aabb(np.vstack((low_positions,high_positions)))
    width=4 if max(author['indices'])>65535 else 2
    sub=copy.deepcopy(mesh['m_SubMeshes'].items()[0])
    for key,value in dict(firstByte=0,indexCount=len(author['indices']),topology=0,baseVertex=0,firstVertex=0,vertexCount=count,localAABB=local_bounds).items():
        sub.set(key,value)
    mesh['m_SubMeshes'].set_items([sub])
    mesh.set('m_IndexFormat',1 if width==4 else 0)
    mesh['m_IndexBuffer'].set_items(np.asarray(author['indices'],dtype='<u4' if width==4 else '<u2').tobytes())
    matrix_schema=mesh['m_BindPose'].array_node().schema.children[1]
    matrix_fields={c.name for c in matrix_schema.children}
    require(matrix_fields=={f'e{r}{c}' for r in range(4) for c in range(4)}, 'native bindpose schema')
    mesh['m_BindPose'].set_items([{f'e{r}{c}':v[r+c*4] for r in range(4) for c in range(4)} for v in author['bindposes']])
    mesh['m_BoneNameHashes'].set_items(author['bone_hashes'])
    per_bone=[]
    for slot,values in enumerate(author['bindposes']):
        selected=np.any((indices==slot)&(weights>0),axis=1)
        if np.any(selected):
            matrix=np.asarray(values).reshape(4,4,order='F')
            low,high=transformed_envelope(low_positions[selected],high_positions[selected],matrix)
        else:
            low=high=np.zeros(3)
        per_bone.append(dict(m_Min=vector(low),m_Max=vector(high)))
    mesh['m_BonesAABB'].set_items(per_bone)
    mesh['m_VariableBoneCountWeights']['m_Data'].set_items([])
    mesh.set('m_MeshCompression',0)
    mesh['m_VertexData'].set('m_VertexCount',count)
    mesh['m_VertexData']['m_Channels'].set_items(channels)
    mesh['m_VertexData'].set('m_DataSize',bytes(vertex_data))
    expand_shaped_frames(mesh)
    for packed in mesh['m_CompressedMesh'].data.values():
        if isinstance(packed.data,dict):
            packed.set('m_NumItems',0)
            packed['m_Data'].set_items(b'')
            packed.set('m_BitSize',0)
            for key in ('m_Range','m_Start'):
                if key in packed.data: packed.set(key,0.)
    mesh['m_CompressedMesh'].set('m_UVInfo',0)
    mesh.set('m_LocalAABB',local_bounds)
    for key in ('m_BakedConvexCollisionMesh','m_BakedTriangleCollisionMesh'):
        require(not mesh[key].items(), 'nonempty native collision payload requires its own rebuild')
    mesh['m_StreamData'].set('offset',0)
    mesh['m_StreamData'].set('size',0)
    mesh['m_StreamData'].set('path','')
    mesh.set('m_BonesPerVertex',4)
    precision.update(weightMaxError=float(np.max(np.abs(quantized/65535-np.asarray([w for w,_ in author['skin']])))),indexBytes=width)
    require(precision['weightMaxError']<=3e-5,'skin quantization error')
    return mesh,precision


def write_shapes(mesh, author):
    shapes = mesh['m_Shapes']
    require(set(shapes.data)=={'vertices','shapes','channels','fullWeights'}, 'native shape schema')
    vertices = author['blend_vertices']
    require(all(0 <= v[0] < author['vertex_count'] and np.all(np.isfinite(np.asarray(v[1:]))) for v in vertices), 'shape vertex range/finite')
    require(len(author['blend_frames']) == len(author['blend_weights']), 'shape frame weight count')
    require(all(not frame[5] for frame in author['blend_frames']), 'additional shape normals unsupported')
    frames = []
    for name, first, count, has_normals, has_tangents, _ in author['blend_frames']:
        require(first+count<=len(vertices), 'shape frame vertex range')
        frames.append(dict(firstVertex=first,vertexCount=count,hasNormals=has_normals,hasTangents=has_tangents))
    channels=[]
    covered=[]
    for name, hash_value, first, count in author['blend_channels']:
        require(count>0 and first+count<=len(frames), 'shape channel frame range')
        require(all(author['blend_frames'][i][0]==name for i in range(first,first+count)), 'shape frame channel identity')
        channels.append(dict(name=name,nameHash=hash_value,frameIndex=first,frameCount=count))
        covered.extend(range(first,first+count))
    require(sorted(covered)==list(range(len(frames))), 'shape frames are not uniquely covered')
    shapes['vertices'].set_items([dict(index=index,vertex=vector(position),normal=vector(normal),tangent=vector(tangent)) for index,position,normal,tangent in vertices])
    shapes['shapes'].set_items(frames)
    shapes['channels'].set_items(channels)
    shapes['fullWeights'].set_items(author['blend_weights'])


def shape_envelope(author):
    """Conservative base + channel envelopes for simultaneous weights in [0,100]."""
    base=np.asarray(author['vertices'],dtype=np.float64).reshape(-1,3)
    lower,upper=base.copy(),base.copy()
    for name,_,first,count in author['blend_channels']:
        channel_low=np.zeros_like(base);channel_high=np.zeros_like(base)
        for frame_index in range(first,first+count):
            _,start,length,*_=author['blend_frames'][frame_index]
            weight=author['blend_weights'][frame_index]
            require(weight>0, 'shape envelope requires positive frame weights')
            delta=np.zeros_like(base)
            records=author['blend_vertices'][start:start+length]
            ids=[v[0] for v in records]
            require(len(set(ids))==len(ids),'duplicate shape frame vertex')
            if records:delta[ids]=np.asarray([v[1] for v in records])*(100./weight)
            channel_low=np.minimum(channel_low,delta);channel_high=np.maximum(channel_high,delta)
        lower+=channel_low;upper+=channel_high
    return lower,upper


def transformed_envelope(lower,upper,matrix):
    center=(lower+upper)*.5;extent=(upper-lower)*.5
    transformed=center@matrix[:3,:3].T+matrix[:3,3]
    radius=extent@np.abs(matrix[:3,:3]).T
    return (transformed-radius).min(0),(transformed+radius).max(0)


def renderer_bounds(author,root_bind):
    lower,upper=shape_envelope(author)
    low,high=transformed_envelope(lower,upper,root_bind)
    return dict(m_Center=vector((low+high)*.5),m_Extent=vector((high-low)*.5))
