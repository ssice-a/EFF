"""Prepare a legacy Format 3 shape-animation experiment without editing its Mod.

Only the old embedded-controls schema used by ZFY is accepted. Native Clips
are built by AnimeStudio's actual writer, not by this metadata inspector.
"""
import argparse
import configparser
import hashlib
import json
import math
from pathlib import Path
import struct


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, size):
        if not 0 <= size <= len(self.data) - self.pos:
            raise ValueError('Truncated source controls')
        value = self.data[self.pos:self.pos + size]
        self.pos += size
        return value

    def n(self):
        return struct.unpack('<Q', self.take(8))[0]

    def count(self):
        value = self.n()
        if value > (len(self.data) - self.pos) // 8:
            raise ValueError('Invalid source metadata count')
        return value

    def d(self):
        value = struct.unpack('<d', self.take(8))[0]
        if not math.isfinite(value):
            raise ValueError('Nonfinite source control')
        return value

    def s(self):
        return self.take(self.n()).decode('utf8')

    def strings(self):
        return [self.s() for _ in range(self.count())]

    def expr(self):
        if not self.n():
            return None
        return [self.n(), self.d(), self.s(), self.expr(), self.expr()]


def metadata(path, kind):
    data = path.read_bytes()
    header = struct.Struct('<16sIIQQ32s')
    magic, version, actual_kind, size, payload, digest = header.unpack_from(data)
    body = data[header.size:header.size + size]
    if (magic != b'EFF_AUTHOR_FILE3' or version != 3 or actual_kind != kind or
            len(data) != header.size + size + payload or hashlib.sha256(body).digest() != digest):
        raise ValueError('Invalid source resource: ' + str(path))
    reader = Reader(body)
    if (reader.s(), reader.n()) != ('EFF_AUTHOR_RESOURCE_METADATA/3', kind):
        raise ValueError('Experiment expects legacy /3 author metadata')
    return reader


def prepare(source, output):
    source, output = source.resolve(), output.resolve()
    ini = configparser.ConfigParser(interpolation=None, delimiters=('=',))
    ini.optionxform = str
    ini.read(source / 'mod.ini', encoding='utf-8-sig')
    if ini['Mod']['format'] != '3':
        raise ValueError('Expected a Format 3 Mod')
    r = metadata(source / ini['ResourceControls']['path'], 6)
    enabled = r.n()
    defaults = {r.s(): r.d() for _ in range(r.count())}
    persistent = r.strings()
    controls = {}
    for _ in range(r.count()):
        name = r.s()
        controls[name] = dict(label=r.s(), minimum=r.d(), maximum=r.d())
    bindings = []
    for _ in range(r.count()):
        row = dict(id=r.s(), variable=r.s(), type=r.s(), key=r.n(), modifiers=r.n(), speed=r.d(), label=r.s())
        row['values'] = [r.d() for _ in range(r.count())]
        bindings.append(row)
    parts = {}
    for _ in range(r.count()):
        name, visible = r.s(), r.expr()
        parts[name] = dict(visible=visible, shapes={r.s(): r.s() for _ in range(r.count())})
    clips = []
    for _ in range(r.count()):
        row = dict(id=r.s(), label=r.s(), speedVariable=r.s(), key=r.n(), modifiers=r.n(),
                   duration=r.d(), speed=r.d(), loop=bool(r.n()), autoplay=bool(r.n()))
        tracks = {}
        for _ in range(r.count()):
            name = r.s()
            tracks[name] = [r.d() for _ in range(r.count())]
        row['tracks'] = tracks
        clips.append(row)
    # This deliberately narrow experiment must not drop other animation kinds.
    if r.strings() or r.count() or r.count() or r.pos != len(r.data):
        raise ValueError('Material/effect controls are outside this shape-only experiment')
    if len(clips) != 1 or not clips[0]['tracks']:
        raise ValueError('Expected exactly one sampled shape animation')
    clip = clips[0]
    targets, bound = [], set()
    for part, declarations in parts.items():
        tracks = [dict(shape=shape, variable=variable, samples=clip['tracks'][variable])
                  for shape, variable in declarations['shapes'].items() if variable in clip['tracks']]
        if not tracks:
            continue
        mesh = ini[ini[part]['mesh']]
        mr = metadata(source / mesh['path'], 1)
        for _ in range(6):
            mr.s()
        mr.n(); mr.n(); mr.strings()
        shape_names = mr.strings()
        if not all(t['shape'] in shape_names and ini[part]['shape.' + t['shape']] == t['variable'] for t in tracks):
            raise ValueError('Animated shape is absent from actual Mesh or INI: ' + part)
        bound.update(t['variable'] for t in tracks)
        targets.append(dict(part=part, mesh=ini[part]['mesh'], tracks=tracks))
    if bound != set(clip['tracks']):
        raise ValueError('Not every animation channel resolves to a Mesh shape')
    output.mkdir(parents=True, exist_ok=True)
    request = dict(package=ini['Mod']['id'], animation=clip['id'], duration=clip['duration'],
                   loop=clip['loop'], targets=targets)
    (output / 'request.json').write_text(json.dumps(request, ensure_ascii=False, indent=2), encoding='utf8')
    files = {p.relative_to(source).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in source.rglob('*') if p.is_file()}
    (output / 'source-receipt.json').write_text(json.dumps(dict(source=str(source), files=files,
        keySwitchEnabled=enabled, defaults=defaults, persistent=persistent, controls=controls,
        bindings=bindings, clips=clips, targets=targets, engineEvaluated=False), ensure_ascii=False, indent=2), encoding='utf8')
    print(f'PASS source mapping: {len(targets)} parts, {len(bound)} channels, duration={clip["duration"]}, key={clip["key"]}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    prepare(args.source, args.output)
