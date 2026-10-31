"""Texture declarations applied to current serialized input, without image decoding."""
from __future__ import annotations

import copy
import math
import struct

from .type_tree import require

TEXTURE_FIELDS = ('m_Width', 'm_Height', 'm_CompleteImageSize', 'm_MipsStripped',
                  'm_TextureFormat', 'm_MipCount', 'm_IsPreProcessed',
                  'm_StreamingMipmaps', 'm_ColorSpace', 'm_TextureSettings')
UNITY_FORMATS = {'RGBA32': 4, 'BC7': 25, 'BC5': 27}
BLOCK_BYTES = {'BC7': 16, 'BC5': 16}

def texture_blocks(texture):
    require(len(texture['pixels']) == texture_byte_size(texture), 'pixel size differs from TEX metadata')
    require(texture['format'] in UNITY_FORMATS, 'unsupported Texture2D format')
    format_id = UNITY_FORMATS[texture['format']]
    if texture['format'] == 'RGBA32':
        metadata = struct.pack('<5I', 1, texture['width'], texture['height'], format_id, texture['mipCount'])
    else:
        digest = bytes.fromhex(texture['sourcePixelsHash'])
        require(len(digest) == 32, 'invalid source Texture pixel digest')
        metadata = struct.pack('<5I', 2, texture['width'], texture['height'], format_id, texture['mipCount']) + digest
    return {'@texture': metadata, 'pixels': texture['pixels']}


def texture_metadata(resource, values):
    blocks = resource['blocks']
    require(set(blocks) in ({'pixels'}, {'@texture', 'pixels'}), 'invalid pixel resource')
    if '@texture' in blocks:
        require(len(blocks['@texture']) in (20, 52), 'invalid TEX metadata length')
        version, width, height, format_id, mips = struct.unpack_from('<5I', blocks['@texture'])
        if version == 1 and len(blocks['@texture']) == 20 and format_id == 4:
            return dict(width=width, height=height, format='RGBA32', mipCount=mips,
                        sourcePixelsHash=None)
        if version == 2 and len(blocks['@texture']) == 52 and format_id in (25, 27):
            return dict(width=width, height=height, format='BC7' if format_id == 25 else 'BC5',
                        mipCount=mips, sourcePixelsHash=blocks['@texture'][20:].hex().upper())
        raise ValueError('unsupported TEX metadata version/format')
    require(all(key in values for key in ('width', 'height', 'format', 'mip_count')),
            'legacy TEX needs INI dimensions; re-export once')
    for key in ('width', 'height', 'mip_count'):
        require(str(values[key]).isdigit(), 'invalid integer texture declaration: ' + key)
    return dict(width=int(values['width']), height=int(values['height']),
                format=values['format'], mipCount=int(values['mip_count']))


def texture_byte_size(texture):
    require(texture['format'] in ('RGBA32', 'BC5', 'BC7'), 'unsupported preconverted texture format')
    width, height, mips = texture['width'], texture['height'], texture['mipCount']
    require(type(width) is int and type(height) is int and 0 < width <= 0xffffffff and 0 < height <= 0xffffffff,
            'invalid texture dimensions')
    require(type(mips) is int and 0 < mips <= max(width.bit_length(), height.bit_length()),
            'invalid texture mip count')
    require(texture['colorSpace'] in (0, 1), 'invalid texture color space')
    require(texture['filter'] in (0, 1, 2) and texture['wrap'] in (0, 1, 2, 3), 'invalid texture sampling mode')
    require(type(texture['aniso']) is int and 0 <= texture['aniso'] <= 16, 'invalid texture anisotropy')
    require(math.isfinite(texture['mipBias']) and abs(texture['mipBias']) <= 3.402823466e38,
            'invalid texture mip bias')
    if texture['format'] == 'RGBA32':
        size = sum(max(width >> i, 1) * max(height >> i, 1) * 4 for i in range(mips))
    else:
        block_bytes = BLOCK_BYTES[texture['format']]
        size = sum(max(1, (max(width >> i, 1) + 3) // 4) *
                   max(1, (max(height >> i, 1) + 3) // 4) * block_bytes
                   for i in range(mips))
    require(size <= 0xffffffff, 'texture payload exceeds native size field')
    return size


def apply_texture_fields(template, texture, payload_size):
    """Apply TEX metadata and INI sampling settings to a native template."""
    require(payload_size == texture_byte_size(texture), 'pixel size does not match TEX metadata')
    require(not template['m_PlatformBlob'].items() and not template['image data'].data,
            'texture platform/inline source unsupported')
    require(template['m_ImageCount'].data == 1 and template['m_TextureDimension'].data == 2,
            'requires Texture2D')
    result = copy.deepcopy(template)
    fields = dict(m_Width=texture['width'], m_Height=texture['height'], m_CompleteImageSize=payload_size,
                  m_MipsStripped=0, m_TextureFormat=UNITY_FORMATS[texture['format']], m_MipCount=texture['mipCount'],
                  m_IsPreProcessed=0, m_StreamingMipmaps=0, m_ColorSpace=texture['colorSpace'])
    for field, value in fields.items():
        result.set(field, value)
    settings = result['m_TextureSettings']
    for field, value in dict(m_FilterMode=texture['filter'], m_Aniso=texture['aniso'], m_MipBias=texture['mipBias'],
                             m_WrapU=texture['wrap'], m_WrapV=texture['wrap'], m_WrapW=texture['wrap']).items():
        settings.set(field, value)
    return result
