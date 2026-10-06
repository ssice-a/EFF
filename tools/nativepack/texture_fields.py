"""Texture declarations applied to current serialized input, without image decoding."""
from __future__ import annotations

import copy
import math
import struct

from .type_tree import require

TEXTURE_FIELDS = ('m_Width', 'm_Height', 'm_CompleteImageSize', 'm_MipsStripped',
                  'm_TextureFormat', 'm_MipCount', 'm_IsPreProcessed',
                  'm_StreamingMipmaps', 'm_ColorSpace', 'm_TextureSettings')

def texture_blocks(texture):
    require(len(texture['pixels']) == texture_byte_size(texture), 'pixel size differs from TEX metadata')
    return {'@texture': struct.pack('<5I', 1, texture['width'], texture['height'], 4, texture['mipCount']),
            'pixels': texture['pixels']}


def texture_metadata(resource, values):
    blocks = resource['blocks']
    require(set(blocks) in ({'pixels'}, {'@texture', 'pixels'}), 'invalid pixel resource')
    if '@texture' in blocks:
        require(len(blocks['@texture']) == 20, 'invalid TEX metadata length')
        version, width, height, format_id, mips = struct.unpack('<5I', blocks['@texture'])
        require(format_id == 4, 'unsupported TEX metadata version/format')
        return dict(width=width, height=height, format='RGBA32', mipCount=mips)
    require(all(key in values for key in ('width', 'height', 'format', 'mip_count')),
            'legacy TEX needs INI dimensions; re-export once')
    for key in ('width', 'height', 'mip_count'):
        require(str(values[key]).isdigit(), 'invalid integer texture declaration: ' + key)
    return dict(width=int(values['width']), height=int(values['height']),
                format=values['format'], mipCount=int(values['mip_count']))


def texture_byte_size(texture):
    require(texture['format'] == 'RGBA32', 'unsupported preconverted texture format')
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
    size = sum(max(width >> i, 1) * max(height >> i, 1) * 4 for i in range(mips))
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
                  m_MipsStripped=0, m_TextureFormat=4, m_MipCount=texture['mipCount'],
                  m_IsPreProcessed=0, m_StreamingMipmaps=0, m_ColorSpace=texture['colorSpace'])
    for field, value in fields.items():
        result.set(field, value)
    settings = result['m_TextureSettings']
    for field, value in dict(m_FilterMode=texture['filter'], m_Aniso=texture['aniso'], m_MipBias=texture['mipBias'],
                             m_WrapU=texture['wrap'], m_WrapV=texture['wrap'], m_WrapW=texture['wrap']).items():
        settings.set(field, value)
    return result
