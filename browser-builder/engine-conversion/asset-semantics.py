"""Fingerprint Pure3D structure while ignoring only lossless PNG packaging."""
import hashlib
import io
import struct
from PIL import Image
from prepare_character_detail import nodes

PNG = b'\x89PNG\r\n\x1a\n'


def semantic_fingerprint(data):
    """Return SHA-256 of exact node order/bodies with PNG pixels canonicalized.

    Only a 0x19002 image-data body containing a size-prefixed PNG is normalized.
    Width, height, every decoded RGBA byte, any trailing body bytes, child order
    and every non-image byte remain part of the fingerprint. Encoded chunk
    length fields are derived framing, not independent semantic content.
    Invalid chunks and invalid PNGs fail closed rather than using a loose hash.
    """
    try:
        tree = nodes(bytes(data))
    except (ValueError, struct.error, RecursionError) as error:
        raise ValueError('Invalid Pure3D structure for semantic verification') from error
    if not tree:
        raise ValueError('Empty Pure3D structure for semantic verification')
    digest = hashlib.sha256(b'SRR-P3D-SEMANTICS-1\0')
    digest.update(struct.pack('<I', len(tree)))

    def visit(node, depth):
        if depth > 128:
            raise ValueError('Pure3D semantic verification exceeds the nesting limit')
        digest.update(struct.pack('<I', node.kind))
        body = node.body
        if node.kind == 0x19002 and len(body) >= 12 and body[4:12] == PNG:
            length, = struct.unpack_from('<I', body)
            if length < 8 or length > len(body) - 4:
                raise ValueError('Invalid size-prefixed PNG body')
            payload, tail = body[4:4 + length], body[4 + length:]
            try:
                with Image.open(io.BytesIO(payload)) as image:
                    if image.format != 'PNG' or image.width * image.height > 16 * 1024 * 1024:
                        raise ValueError('Invalid PNG dimensions or format')
                    image.verify()
                with Image.open(io.BytesIO(payload)) as image:
                    pixels = image.convert('RGBA')
                    width, height = pixels.size
                    rgba = pixels.tobytes()
            except Exception as error:
                raise ValueError('Invalid PNG during semantic verification') from error
            digest.update(b'P')
            digest.update(struct.pack('<IIQ', width, height, len(rgba)))
            digest.update(rgba)
            digest.update(struct.pack('<Q', len(tail)))
            digest.update(tail)
        else:
            digest.update(b'R')
            digest.update(struct.pack('<Q', len(body)))
            digest.update(body)
        digest.update(struct.pack('<I', len(node.children)))
        for child in node.children:
            visit(child, depth + 1)

    for root in tree:
        visit(root, 0)
    return digest.hexdigest()
