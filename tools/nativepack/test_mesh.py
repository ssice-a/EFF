"""Static author geometry must not invent a skin palette or vertex stream."""
import unittest
import numpy as np
from . import mesh
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
