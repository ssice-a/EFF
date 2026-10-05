"""Check actual captured afterimage draws for an empty palette/collapsed mesh.

Input is the matching-SDK renderdoc-cli draw-inputs export, not plugin logs.
Optionally compare its GPU palette byte-for-byte with the adapter's bounded
caller-payload evidence. Pixel visibility still requires visual verification.
"""
import argparse
import json
from pathlib import Path
import numpy as np


def audit(directory, index_count, expected_palette=None):
    expected = expected_palette.read_bytes() if expected_palette else None
    rows = []
    for file in sorted(directory.glob('event*/draw-inputs.json')):
        state = json.loads(file.read_text())
        if state['indexCount'] != index_count:
            continue
        palette = next((d for d in state.get('descriptors', [])
                        if d['set'] == 3 and d['binding'] == 0), None)
        if not palette:
            continue  # Main character/shadow passes use the game's other palette.
        path = file.parent
        data = (path / palette['file']).read_bytes()
        if len(data) < 112 or (len(data) - 16) % 48:
            raise ValueError('Captured native palette extent is not header + 3x4 matrices')
        payload = data[16:]
        matrices = np.frombuffer(payload, '<f4').reshape(-1, 3, 4)
        post = state['postVertex']
        raw = (path / post['file']).read_bytes()
        if post['stride'] < 16 or len(raw) % post['stride']:
            raise ValueError('Captured post-VS vertex extent')
        vertices = np.ndarray((len(raw) // post['stride'], 4), dtype='<f4',
                              buffer=raw, strides=(post['stride'], 4))
        # NaNs compare unequal even when every invocation emits the same NaN
        # tuple. Canonicalize them for geometry cardinality, and reject them
        # independently via postVertexFinite.
        canonical = vertices.copy()
        canonical[~np.isfinite(canonical)] = 0
        unique = len(np.unique(canonical, axis=0))
        palette_finite = bool(np.isfinite(matrices).all())
        nonzero = int(np.count_nonzero(matrices))
        skin_linear_nonzero = int(np.count_nonzero(matrices[1:, :, :3]))
        post_finite = bool(np.isfinite(vertices).all())
        row = dict(event=state['event'], indexCount=index_count,
                   vertices=len(vertices), paletteMatrices=len(matrices),
                   paletteFinite=palette_finite, paletteNonzeroComponents=nonzero,
                   skinLinearNonzeroComponents=skin_linear_nonzero,
                   postVertexFinite=post_finite, postVertexUnique=unique,
                   postVertexCollapsed=unique <= 1,
                   paletteEqualsExpected=None if expected is None else payload == expected,
                   postRange=[[float(finite.min()), float(finite.max())] if len(finite) else [None, None]
                              for column in vertices.T for finite in [column[np.isfinite(column)]]],
                   passed=palette_finite and skin_linear_nonzero > 0 and post_finite and unique > 1 and
                          state['maxIndex'] < len(vertices) and
                          (expected is None or payload == expected))
        rows.append(row)
    return dict(format='EFF_AFTERIMAGE_GPU_PALETTE_AUDIT', version=1,
                draws=rows, passed=bool(rows) and all(r['passed'] for r in rows),
                visualPixelVerification=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gpu-dir', type=Path, required=True)
    parser.add_argument('--index-count', type=int, required=True)
    parser.add_argument('--expected-palette', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = audit(args.gpu_dir, args.index_count, args.expected_palette)
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    print(json.dumps(dict(passed=report['passed'], draws=len(report['draws']),
                          uniquePostVertices=[r['postVertexUnique'] for r in report['draws']])))
    raise SystemExit(0 if report['passed'] else 1)
