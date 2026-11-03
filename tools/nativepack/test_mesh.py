"""Static author geometry must not invent a skin palette or vertex stream."""
import unittest
from unittest.mock import patch
import numpy as np
from . import mesh
from . import migrate_format2
from .mesh import skin_stream
from types import SimpleNamespace


class FrameStreamTests(unittest.TestCase):
    def channels(self, packed):
        rows = [dict(format=0, dimension=0) for _ in range(14)]
        rows[0]['dimension'] = 3
        rows[1]['dimension'] = 0x31 if packed else 3
        rows[2]['dimension'] = 0 if packed else 4
        return [{k: SimpleNamespace(data=v) for k, v in row.items()} for row in rows]

    def test_float_normals_and_tangents_keep_source_storage(self):
        p=np.array([[1.,2.,3.],[-2.,3.,4.]])
        n=np.array([[0.,0.,1.],[0.,1.,0.]])
        t=np.array([[1.,0.,0.,-1.],[1.,0.,0.,1.]])
        data, dimension, _=mesh.frame_stream(self.channels(False),p,n,t)
        self.assertEqual((data.dtype.str,data.shape,dimension),('<f4',(2,10),3))
        np.testing.assert_array_equal(data,np.column_stack((p,n,t)))

    def test_packed_frames_still_decode_within_precision(self):
        p=np.array([[1.,2.,3.]])
        n=np.array([[0.,0.,1.]])
        t=np.array([[1.,0.,0.,-1.]])
        data, dimension, precision=mesh.frame_stream(self.channels(True),p,n,t)
        self.assertEqual((data.shape,dimension),((1,4),0x31))
        actual_n, actual_t=mesh.unpack_frames(data[:,3].view('<u4'))
        np.testing.assert_allclose(actual_n,n,atol=.006)
        np.testing.assert_allclose(actual_t,t,atol=.015)

    def test_float_storage_preserves_zero_tangent_from_blender(self):
        data, dimension, _=mesh.frame_stream(self.channels(False),np.zeros((1,3)),
            np.array([[0.,0.,1.]]),np.zeros((1,4)))
        self.assertEqual(dimension,3)
        np.testing.assert_array_equal(data[:,6:10],np.zeros((1,4)))

    def test_unknown_normal_encoding_is_rejected(self):
        channels=self.channels(False); channels[1]['dimension'].data=2
        with self.assertRaisesRegex(ValueError,'frame layout'):
            mesh.frame_stream(channels,np.zeros((1,3)),np.array([[0.,0.,1.]]),np.array([[1.,0.,0.,1.]]))

    def test_shaped_frame_expansion_retains_native_uv_values(self):
        class Node:
            def __init__(self, value):
                self.data = value

            def __getitem__(self, key):
                value = self.data[key]
                return value if isinstance(value, Node) else Node(value)

            def items(self):
                return [value if isinstance(value, Node) else Node(value)
                        for value in self.data]

            def set(self, key, value):
                self.data[key] = value

        count = 3
        channels = [dict(stream=0, offset=0, format=0, dimension=0)
                    for _ in range(14)]
        channels[0] = dict(stream=0, offset=0, format=0, dimension=3)
        channels[1] = dict(stream=0, offset=12, format=0, dimension=0x31)
        channels[4] = dict(stream=1, offset=0, format=0, dimension=2)
        positions = np.asarray([[0., 0., 0.], [1., 0., 0.], [0., 1., 0.]], dtype='<f4')
        frames = np.asarray([0x40000000] * count, dtype='<u4')
        stream0 = np.column_stack((positions, frames.view('<f4'))).astype('<f4').tobytes()
        uvs = np.asarray([[.1, .2], [.8, .9], [.4, .7]], dtype='<f4')
        stream1 = uvs.tobytes() + bytes(8)
        native = Node({
            'm_VertexData': {
                'm_VertexCount': count,
                'm_Channels': channels,
                'm_DataSize': stream0 + stream1,
            },
            'm_Shapes': {'channels': [{}]},
        })

        fields = {
            'm_VertexData': b'legacy vertex field',
            'm_IndexBuffer': b'unchanged field',
        }
        with patch.object(migrate_format2, 'encode', return_value=b'expanded vertex field'):
            migrate_format2._prepare_migrated_mesh_fields(native, fields)

        vertex = native['m_VertexData']
        expanded = vertex['m_DataSize'].data
        actual = np.ndarray((count, 2), dtype='<f4', buffer=expanded,
                            offset=128, strides=(8, 4))
        np.testing.assert_array_equal(actual, uvs)
        self.assertEqual(vertex['m_Channels'].items()[1]['dimension'].data, 3)
        self.assertEqual(fields['m_VertexData'], b'expanded vertex field')
        self.assertEqual(fields['m_IndexBuffer'], b'unchanged field')

    def test_migration_keeps_unshaped_native_mesh_fields_unchanged(self):
        fields = {
            'm_VertexData': b'original vertex bytes',
            'm_IndexBuffer': b'original index bytes',
        }
        original = fields.copy()
        with patch.object(mesh, 'expand_shaped_frames', return_value=False), \
                patch.object(migrate_format2, 'encode') as encode:
            migrate_format2._prepare_migrated_mesh_fields(object(), fields)
        self.assertEqual(fields, original)
        encode.assert_not_called()


class TextureCoordinateTests(unittest.TestCase):
    def test_native_uvs_and_tangent_handedness_preserve_author_data(self):
        source_uvs = [
            [.1, .2, .8, .9],
            [.3, .4, .7, .6, .2, .1],
            [.5, .25, .75, .1, .8, .6, .4, .9],
            [], [], [], [], [],
        ]
        source_tangents = [1., 0., 0., 1., 0., 1., 0., -1.]
        original_uvs = [values[:] for values in source_uvs]
        original_tangents = source_tangents[:]

        converted, tangents = mesh.game_texture_coordinates(
            source_uvs, source_tangents, 2)

        np.testing.assert_allclose(converted[0], [[.1, .2], [.8, .9]], atol=3e-8)
        np.testing.assert_allclose(converted[1], [[.3, .4, .7], [.6, .2, .1]], atol=3e-8)
        np.testing.assert_allclose(converted[2], [[.5, .25, .75, .1],
                                                  [.8, .6, .4, .9]], atol=3e-8)
        self.assertEqual(converted[3:], [None] * 5)
        np.testing.assert_array_equal(tangents.reshape(2, 4)[:, :3],
                                      np.asarray(source_tangents).reshape(2, 4)[:, :3])
        np.testing.assert_array_equal(tangents.reshape(2, 4)[:, 3], [1., -1.])
        self.assertEqual(source_uvs, original_uvs)
        self.assertEqual(source_tangents, original_tangents)

    def test_blender_author_uvs_and_tangent_sign_convert_back_to_native(self):
        source_uvs = [
            [.1, .2, .8, .9],
            [.3, .4, .7, .6, .2, .1],
            [], [], [], [], [], [],
        ]
        source_tangents = [1., 0., 0., -1., 0., 1., 0., 1.]
        original_uvs = [values[:] for values in source_uvs]
        original_tangents = source_tangents[:]

        converted, tangents = mesh.game_texture_coordinates(
            source_uvs, source_tangents, 2, coordinate='Blender')

        np.testing.assert_allclose(converted[0], [[.1, .8], [.8, .1]], atol=3e-8)
        np.testing.assert_allclose(converted[1], [[.3, .6, .7], [.6, .8, .1]], atol=3e-8)
        np.testing.assert_array_equal(tangents.reshape(2, 4)[:, 3], [1., -1.])
        self.assertEqual(source_uvs, original_uvs)
        self.assertEqual(source_tangents, original_tangents)

    def test_blender_author_without_uv0_does_not_change_tangent_sign(self):
        converted, tangents = mesh.game_texture_coordinates(
            [[], [.1, .2, .7, .8]] + [[]] * 6,
            [1., 0., 0., 1., 1., 0., 0., -1.], 2,
            coordinate='Blender')
        np.testing.assert_allclose(converted[1], [[.1, .8], [.7, .2]])
        np.testing.assert_array_equal(tangents.reshape(2, 4)[:, 3], [1., -1.])

    def test_uv_channels_without_uv0_preserve_tangent_handedness(self):
        channels = [[], [.1, .2, .7, .8]] + [[]] * 6
        converted, tangents = mesh.game_texture_coordinates(
            channels, [1., 0., 0., 1., 1., 0., 0., -1.], 2)
        np.testing.assert_allclose(converted[1], [[.1, .2], [.7, .8]], atol=3e-8)
        np.testing.assert_array_equal(tangents.reshape(2, 4)[:, 3], [1., -1.])

    def test_native_uvs_reject_invalid_channel_shapes(self):
        with self.assertRaisesRegex(ValueError, 'UV shape'):
            mesh.game_texture_coordinates([[.1, .2, .3], *([[]] * 7)],
                                          [1., 0., 0., 1.], 2)

    def test_legacy_mesh_decode_flips_v_and_tangent_handedness_once(self):
        class Node:
            def __init__(self, value):
                self.data = value

            def __getitem__(self, key):
                value = self.data[key]
                return value if isinstance(value, Node) else Node(value)

            def items(self):
                if isinstance(self.data, (bytes, bytearray)):
                    return self.data
                return [value if isinstance(value, Node) else Node(value)
                        for value in self.data]

            def plain(self):
                return self.data

        positions = np.asarray([[0., 0., 0.], [1., 0., 0.], [0., 1., 0.]], dtype='<f4')
        normals = np.asarray([[0., 0., 1.]] * 3, dtype='<f4')
        tangents = np.asarray([[1., 0., 0., -1.], [1., 0., 0., 1.],
                               [1., 0., 0., -1.]], dtype='<f4')
        uv = np.asarray([[.1, .2], [.8, .9], [.4, .7]], dtype='<f4')
        stream0 = np.column_stack((positions, normals, tangents)).astype('<f4').tobytes()
        stream0 += bytes((-len(stream0)) % 16)
        stream1 = uv.tobytes()
        vertex_data = stream0 + stream1
        vertex_data += bytes((-len(vertex_data)) % 16)

        channels = [dict(stream=0, offset=0, format=0, dimension=0) for _ in range(14)]
        channels[0] = dict(stream=0, offset=0, format=0, dimension=3)
        channels[1] = dict(stream=0, offset=12, format=0, dimension=3)
        channels[2] = dict(stream=0, offset=24, format=0, dimension=4)
        channels[4] = dict(stream=1, offset=0, format=0, dimension=2)
        native_mesh = Node({
            'm_VertexData': Node({
                'm_VertexCount': Node(3),
                'm_Channels': Node(channels),
                'm_DataSize': Node(vertex_data),
            }),
            'm_Name': Node('legacy'),
            'm_IndexBuffer': Node(np.asarray([0, 1, 2], dtype='<u2').tobytes()),
            'm_IndexFormat': Node(0),
            'm_SubMeshes': Node([dict(
                topology=0, firstByte=0, indexCount=3, baseVertex=0,
                firstVertex=0, vertexCount=3,
            )]),
            'm_BindPose': Node([]),
            'm_BoneNameHashes': Node([]),
            'm_Shapes': Node({
                'vertices': [], 'shapes': [], 'channels': [], 'fullWeights': [],
            }),
        })

        native_vertex_data = native_mesh['m_VertexData']['m_DataSize'].data
        decoded = mesh.decode_native_mesh(native_mesh, [])

        expected_uv = uv.copy()
        expected_uv[:, 1] = 1.0 - expected_uv[:, 1]
        expected_tangents = tangents.copy()
        expected_tangents[:, 3] *= -1.0
        np.testing.assert_allclose(decoded['uvs'][0], expected_uv.reshape(-1), rtol=0, atol=3e-8)
        np.testing.assert_allclose(decoded['tangents'], expected_tangents.reshape(-1), rtol=0, atol=1e-7)
        self.assertEqual(native_mesh['m_VertexData']['m_DataSize'].data,
                         native_vertex_data)
        self.assertEqual(decoded['coordinate'], 'Blender')
        restored_uvs, restored_tangents = mesh.game_texture_coordinates(
            decoded['uvs'], decoded['tangents'], 3,
            coordinate=decoded['coordinate'])
        np.testing.assert_allclose(restored_uvs[0], uv, rtol=0, atol=3e-8)
        np.testing.assert_allclose(restored_tangents, tangents, rtol=0, atol=1e-7)

        zero_uv_stream = bytes(uv.nbytes)
        zero_uv_data = stream0 + zero_uv_stream
        zero_uv_data += bytes((-len(zero_uv_data)) % 16)
        native_mesh['m_VertexData']['m_DataSize'].data = zero_uv_data
        zero_uv_decoded = mesh.decode_native_mesh(native_mesh, [])
        np.testing.assert_array_equal(
            np.asarray(zero_uv_decoded['uvs'][0]).reshape(3, 2)[:, 1], [1., 1., 1.])
        np.testing.assert_allclose(
            np.asarray(zero_uv_decoded['tangents']).reshape(3, 4)[:, 3],
            -tangents[:, 3], rtol=0, atol=1e-7)


class SkinStreamTests(unittest.TestCase):
    def static(self):
        return dict(vertex_count=3, skin=[], bone_paths=[], bindposes=[], bone_hashes=[])

    def test_static_is_empty(self):
        stream, weights, indices, error = skin_stream(self.static())
        self.assertEqual(stream, b'')
        self.assertEqual(weights.shape, (0, 4))
        self.assertEqual(indices.shape, (0, 4))
        self.assertEqual(error, 0.)

    def test_skinned_storage_and_precision_are_preserved(self):
        author = dict(vertex_count=1, skin=[([.1, .2, .3, .4], [0, 1, 2, 3])],
                      bone_paths=['a', 'b', 'c', 'd'], bindposes=[[]]*4, bone_hashes=[1]*4)
        stream, _, indices, error = skin_stream(author)
        self.assertEqual(len(stream), 12)
        self.assertEqual(np.frombuffer(stream[:8], dtype='<u2').sum(), 65535)
        self.assertEqual(list(stream[8:]), indices[0].tolist())
        self.assertLessEqual(error, 3e-5)

    def test_static_rejects_fake_weights(self):
        author = self.static()
        author['skin'] = [([1, 0, 0, 0], [0]*4)]*3
        with self.assertRaisesRegex(ValueError, 'unskinned'):
            skin_stream(author)

    def test_inconsistent_metadata_is_rejected(self):
        author = self.static()
        author['bindposes'] = [[]]
        with self.assertRaisesRegex(ValueError, 'metadata'):
            skin_stream(author)

    def test_skinned_invalid_vertex_or_palette_is_rejected(self):
        author = dict(vertex_count=1, skin=[], bone_paths=['a'], bindposes=[[]], bone_hashes=[1])
        with self.assertRaisesRegex(ValueError, 'vertex count'):
            skin_stream(author)
        author['skin'] = [([1, 0, 0, 0], [1]*4)]
        with self.assertRaisesRegex(ValueError, 'indices'):
            skin_stream(author)


if __name__ == '__main__':
    unittest.main()
