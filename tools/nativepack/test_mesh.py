"""Static author geometry must not invent a skin palette or vertex stream."""
import unittest
import numpy as np
from .mesh import skin_stream


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
