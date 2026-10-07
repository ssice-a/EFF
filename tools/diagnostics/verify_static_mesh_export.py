"""Read-only export regression against a supplied real Endfield Mesh schema.

No Blender scene is touched; no author or game resource is written.
"""
import argparse
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.nativepack.author_source import AuthorCore, AuthorSource
from tools.nativepack.mesh import native_mesh
from tools.nativepack.type_tree import encode, decode

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--core', type=Path, required=True)
    args = parser.parse_args()
    source = AuthorSource(args.source, AuthorCore(args.core))
    resource = next(r for r in source.manifest['resources'] if r['type'] == 'Mesh')
    template = source.tree(resource)
    author = dict(vertex_count=3, vertices=[0., 0., 0., 1., 0., 0., 0., 1., 0.],
                  normals=[0., 0., 1.]*3, tangents=[1., 0., 0., 1.]*3,
                  indices=[0, 1, 2], additional=[], uvs=[], colors=[], skin=[],
                  bone_paths=[], bindposes=[], bone_hashes=[], blend_vertices=[],
                  blend_frames=[], blend_weights=[], blend_channels=[])
    for uvs in ([], [[0., 0., 1., 0., 0., 1.]]):
        author['uvs'] = uvs
        mesh, precision = native_mesh(template, author, 'static-export-test')
        assert mesh['m_BonesPerVertex'].data == 0
        assert mesh['m_RootBoneNameHash'].data == 0
        assert not mesh['m_BindPose'].items() and not mesh['m_BoneNameHashes'].items()
        assert not mesh['m_BonesAABB'].items()
        assert all(mesh['m_VertexData']['m_Channels'].items()[i]['dimension'].data == 0 for i in (12, 13))
        assert len(mesh['m_VertexData']['m_DataSize'].data) == (80 if uvs else 48)
        assert mesh['m_LocalAABB']['m_Center']['x'].data == .5
        assert precision['weightMaxError'] == 0.
        data = encode(mesh)
        assert encode(decode(mesh.schema, data)) == data
    print('PASS real Mesh TypeTree: static geometry with/without UV, local bounds, no skin channels/palette, complete serialization roundtrip; source unchanged')

if __name__ == '__main__':
    main()
