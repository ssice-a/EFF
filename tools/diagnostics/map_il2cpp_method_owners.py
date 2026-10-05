"""Read-only metadata-v29 / PE method-owner lookup for inspected RVAs.

Validates assembly names, method owners/tokens and CodeGenModule pointer counts.
RVAs are evidence for this file only; this tool does not install runtime hooks.
"""
import argparse
import json
import mmap
import struct
from pathlib import Path

from inspect_native_shapes import Image


def lookup(metadata, image, targets, type_stride, assemblies=None):
    if struct.unpack_from('<II', metadata) != (0xFAB11BAF, 29):
        raise ValueError('Expected standard metadata v29')

    def table(pair, stride):
        offset, size = struct.unpack_from('<II', metadata, 8 + pair * 8)
        if offset + size > len(metadata) or size % stride:
            raise ValueError('Invalid metadata table layout')
        return offset, size // stride

    strings, string_size = table(2, 1)
    types, type_count = table(19, type_stride)
    methods, method_count = table(5, 32)
    parameters, parameter_count = table(10, 12)
    images, image_count = table(20, 40)

    def name(index):
        if not 0 <= index < string_size:
            raise ValueError('Invalid string index')
        end = metadata.find(b'\0', strings + index, strings + string_size)
        if end < 0:
            raise ValueError('Unterminated string')
        return metadata[strings + index:end].decode('utf-8')

    matches, modules = [], []
    for assembly_index in range(image_count):
        definition = struct.unpack_from('<10I', metadata, images + 40 * assembly_index)
        assembly = name(definition[0])
        if assemblies and assembly not in assemblies:
            continue
        first, count = definition[2:4]
        if first + count > type_count:
            raise ValueError('Invalid image type range')
        by_slot = {}
        for type_index in range(first, first + count):
            offset = types + type_stride * type_index
            fields = struct.unpack_from('<16I', metadata, offset)
            counts = struct.unpack_from('<8H', metadata, offset + type_stride - 24)
            if counts[0] and fields[9] + counts[0] > method_count:
                raise ValueError('Invalid type method range')
            for index in range(fields[9], fields[9] + counts[0]):
                values = struct.unpack_from('<6I4H', metadata, methods + 32 * index)
                if values[1] != type_index or values[5] >> 24 != 6:
                    raise ValueError('Method owner/token mismatch')
                slot = (values[5] & 0xFFFFFF) - 1
                if slot < 0 or slot in by_slot:
                    raise ValueError('Invalid or duplicate method token slot')
                by_slot[slot] = (type_index, fields, values)
        if not by_slot:
            continue
        expected_count = max(by_slot) + 1
        module_candidates = []
        start = 0
        while True:
            offset = image.data.find(assembly.encode('ascii') + b'\0', start)
            if offset < 0:
                break
            start = offset + len(assembly) + 1
            if offset and image.data[offset - 1] != 0:
                continue
            name_va = image.base + image.pe.get_rva_from_offset(offset)
            reference = 0
            while True:
                reference = image.data.find(struct.pack('<Q', name_va), reference)
                if reference < 0:
                    break
                module_offset = reference
                reference += 8
                if module_offset % 8 or module_offset + 24 > len(image.data):
                    continue
                _, pointer_count, pointer_va = struct.unpack_from('<QQQ', image.data, module_offset)
                if pointer_count != expected_count:
                    continue
                try:
                    pointer_offset = image.offset(pointer_va - image.base)
                    end_offset = image.offset(pointer_va - image.base + 8 * pointer_count - 1)
                except Exception:
                    continue
                if end_offset != pointer_offset + 8 * pointer_count - 1:
                    continue
                # Nonzero entries must be executable pointers in the inspected PE.
                pointers = struct.unpack_from(f'<{pointer_count}Q', image.data, pointer_offset)
                code_ranges = [(s.VirtualAddress, s.VirtualAddress + s.Misc_VirtualSize)
                               for s in image.code]
                if any(p and not any(lo <= p - image.base < hi for lo, hi in code_ranges)
                       for p in pointers):
                    continue
                module_candidates.append((module_offset, pointers))
        if len(module_candidates) > 1:
            raise ValueError(f'Ambiguous CodeGenModule for {assembly}')
        if not module_candidates:
            continue
        module_offset, pointers = module_candidates[0]
        modules.append(dict(assembly=assembly,
                            rva=hex(image.pe.get_rva_from_offset(module_offset)),
                            pointer_count=len(pointers), metadata_methods=len(by_slot)))
        for slot, pointer in enumerate(pointers):
            if not pointer or pointer - image.base not in targets:
                continue
            if slot not in by_slot:
                raise ValueError('Matched pointer has no metadata method')
            type_index, fields, values = by_slot[slot]
            if values[9] and values[3] + values[9] > parameter_count:
                raise ValueError('Invalid parameter range')
            args = []
            for index in range(values[3], values[3] + values[9]):
                parameter = struct.unpack_from('<3I', metadata, parameters + 12 * index)
                if parameter[1] >> 24 != 8:
                    raise ValueError('Invalid parameter token')
                args.append(dict(name=name(parameter[0]), type_index=parameter[2]))
            matches.append(dict(rva=hex(pointer - image.base), assembly=assembly,
                                namespace=name(fields[1]), owner=name(fields[0]),
                                type_index=type_index, method=name(values[0]),
                                token=hex(values[5]), static=bool(values[6] & 0x10),
                                return_type_index=values[2], parameters=args))
    return dict(image=str(image.path), type_stride=type_stride,
                requested=[hex(t) for t in sorted(targets)], modules=modules,
                matches=matches,
                unresolved=[hex(t) for t in sorted(targets - {int(m['rva'], 16) for m in matches})])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('metadata', type=Path)
    parser.add_argument('image', type=Path)
    parser.add_argument('--rva', type=lambda s: int(s, 0), action='append', required=True)
    parser.add_argument('--type-stride', type=int, choices=(88, 92), required=True)
    parser.add_argument('--assembly', action='append', help='Restrict lookup to named assemblies')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    with args.metadata.open('rb') as meta_file, args.image.open('rb') as image_file:
        with mmap.mmap(meta_file.fileno(), 0, access=mmap.ACCESS_READ) as metadata:
            with mmap.mmap(image_file.fileno(), 0, access=mmap.ACCESS_READ) as data:
                report = lookup(metadata, Image(args.image, data), set(args.rva), args.type_stride, args.assembly)
    with args.output.open('x', encoding='utf-8') as output:
        output.write(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(dict(matches=report['matches'], unresolved=report['unresolved']), ensure_ascii=False))


if __name__ == '__main__':
    main()
