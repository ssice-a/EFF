"""Audit independently decoded replacement Mesh draw ranges and raw topology.

This examines serialized data. It does not observe GPU bindings or a draw.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

import numpy as np

from audit_deployed_skinning import load_files
from analyze_native_render_record import stream_layout
from nativepack.ini_reader import read_package
from nativepack.payload import read_payload
from nativepack.type_tree import require


def audit(mesh, part, package):
    vertex = mesh['m_VertexData']
    count = vertex['m_VertexCount'].data
    channels = [tuple(c[k].data for k in ('stream', 'offset', 'format', 'dimension'))
                for c in vertex['m_Channels'].items()]
    streams = stream_layout(channels, count)
    raw = vertex['m_DataSize'].data
    for stream in streams:
        require(stream['start'] + count * stream['stride'] <= len(raw), 'vertex stream exceeds data range')
        require(stream['start'] % 16 == 0, 'vertex stream alignment')
    require(channels[0] == (0, 0, 0, 3) and streams[0]['stride'] == 16, 'position stream interpretation')
    require(count == part['vertices'], 'vertex count differs from author')
    index_format = mesh['m_IndexFormat'].data
    require(index_format in (0, 1), 'index format')
    width = (2, 4)[index_format]
    indices_raw = bytes(mesh['m_IndexBuffer'].items())
    require(len(indices_raw) % width == 0, 'partial index element')
    indices = np.frombuffer(indices_raw, dtype='<u2' if width == 2 else '<u4')
    author_index_field = next(f for f in part['nativeFields'] if f['name'] == 'm_IndexBuffer')
    author_indices = read_payload(package, author_index_field)
    author_index_bytes = struct.unpack_from('<I', author_indices)[0]
    require(author_index_bytes == len(indices_raw) and
            author_indices[4:4 + author_index_bytes] == indices_raw, 'triangle topology differs from author')
    submeshes = mesh['m_SubMeshes'].items()
    require(len(submeshes) == 1, 'replacement part submesh count')
    submesh = submeshes[0].plain()
    require(submesh['topology'] == 0 and submesh['indexCount'] == part['indices'] == len(indices) and
            submesh['indexCount'] % 3 == 0, 'triangle draw count/topology')
    require(submesh['firstByte'] == submesh['baseVertex'] == submesh['firstVertex'] == 0 and
            submesh['vertexCount'] == count, 'replacement draw offset/range')
    require(np.all(indices < count), 'triangle references nonexistent vertex')
    positions = np.ndarray((count, 3), dtype='<f4', buffer=raw, strides=(16, 4)).astype(np.float64)
    triangles = indices.reshape(-1, 3)
    points = positions[triangles]
    edges = np.linalg.norm(points - np.roll(points, 1, axis=1), axis=2)
    require(np.all(np.isfinite(edges)), 'nonfinite topology edge')
    return dict(vertices=count, indexCount=len(indices), indexBits=width * 8,
                indexBufferSha256=hashlib.sha256(indices_raw).hexdigest().upper(),
                topologyExactlyMatchesAuthor=True, submesh=submesh, streams=streams,
                vertexDataBytes=len(raw), maxIndex=int(indices.max()),
                longestUnskinnedTriangleEdge=float(edges.max()), actualGpuDrawObserved=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('schema', 'mesh-audit', 'mod', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'draw input audit output exists')
    files = load_files(args.schema)
    package = read_package(args.mod)
    parts = {part['partId']: part for part in package['parts']}
    association = json.loads(args.mesh_audit.read_text(encoding='utf-8-sig'))
    result = dict(format='EFF_MESH_DRAW_INPUT_AUDIT', version=1,
                  actualGpuDrawObserved=False, originalVisualBugFixed=False, meshes=[])
    for row in association['meshes']:
        report = audit(files[row['cab']].tree(row['mesh']), parts[row['part']], package)
        result['meshes'].append(dict(cab=row['cab'], mesh=row['mesh'], part=row['part'], **report))
    result.update(meshObjects=len(result['meshes']), authorParts=len({row['part'] for row in result['meshes']}),
                  allTopologyAndDrawRangesVerified=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k != 'meshes'}, ensure_ascii=False))


if __name__ == '__main__':
    main()
