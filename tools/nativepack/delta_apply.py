"""Strict offline implementation of the declared native geometry delta contract.

This reconstructs serialized input, never sets a running Unity Mesh.
"""
import copy
from .type_tree import decode,require
from .mesh import expand_shaped_frames

GEOMETRY_FIELDS=('m_SubMeshes','m_Shapes','m_BindPose','m_BoneNameHashes','m_BonesAABB',
                 'm_VariableBoneCountWeights','m_MeshCompression','m_IndexFormat','m_IndexBuffer',
                 'm_VertexData','m_CompressedMesh','m_LocalAABB','m_StreamData','m_BonesPerVertex')


def apply_mesh_fields(template,part,read_field):
    require({f['name'] for f in part['nativeFields']}==set(GEOMETRY_FIELDS) and len(part['nativeFields'])==len(GEOMETRY_FIELDS),'incomplete or duplicate geometry delta fields')
    source_channels=template['m_VertexData']['m_Channels'].items()
    require(len(source_channels)==14 and source_channels[1]['dimension'].data==0x31,'native packed-frame source channel contract')
    result=copy.deepcopy(template)
    for field in part['nativeFields']:
        name=field['name'];result.data[name]=decode(result[name].schema,read_field(field))
    channels=result['m_VertexData']['m_Channels'].items()
    require(len(channels)==14,'native author vertex channels')
    for i in range(4,12):
        dimension=channels[i]['dimension'].data&0x0f
        if dimension:channels[i].set('dimension',(source_channels[i]['dimension'].data&0xf0)|dimension)
    expand_shaped_frames(result)
    return result
