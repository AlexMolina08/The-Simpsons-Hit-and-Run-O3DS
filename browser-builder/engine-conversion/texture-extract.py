"""Expose the existing PC texture parser to the browser's exact WASM converters."""
import hashlib
import struct

from prepare_native_textures import HEADER, VERSION, texture_images, variants


def texture_requests(data, accepted_keys):
    """Return only baseline variants, retaining the production source identity."""
    data = bytes(data)
    accepted = set(accepted_keys)
    result = []
    seen = set()
    for w, h, kind, raw, wire_format, source_bytes in texture_images(data, False):
        for width, height, levels in variants(w, h):
            size = sum((width >> i) * (height >> i) * 2 for i in range(levels + 1))
            source_kind = 256 if kind == 0 else kind
            prefix = HEADER.pack(0x3158544E, VERSION, w, h, source_kind,
                                 width, height, wire_format, levels,
                                 len(source_bytes), size)
            prefix += hashlib.sha256(source_bytes).digest()
            key = hashlib.sha256(prefix).hexdigest()
            if key not in accepted or key in seen:
                continue
            seen.add(key)
            result.append(dict(key=key, w=w, h=h, kind=kind, dw=width, dh=height,
                               format=wire_format, levels=levels, data=raw,
                               prefix=prefix, bytes=size))
    return result
