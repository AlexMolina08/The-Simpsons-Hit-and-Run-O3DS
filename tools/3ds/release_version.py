"""Version handling shared by CIA packaging and the local release launcher."""
import re
import struct

PATTERN = re.compile(r'v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-(rc|beta|alpha)\.(0|[1-9][0-9]*))?\Z')


def parse(value):
    match = PATTERN.fullmatch(value)
    if not match:
        raise ValueError('Use MAJOR.MINOR.PATCH, optionally followed by -rc.N, -beta.N or -alpha.N')
    major, minor, patch = map(int, match.group(1, 2, 3))
    if major > 63 or minor > 63 or patch > 15:
        raise ValueError('The CIA version fields allow major/minor 0–63 and patch 0–15')
    version = f'{major}.{minor}.{patch}'
    if match.group(4):
        version += f'-{match.group(4)}.{match.group(5)}'
    return version, (major, minor, patch)


def next_version(tags):
    versions = []
    for tag in tags:
        try:
            versions.append(parse(tag)[1])
        except ValueError:
            continue
    major, minor, patch = max(versions, default=(0, 1, 0))
    patch += 1
    if patch > 15:
        patch, minor = 0, minor + 1
    if minor > 63:
        minor, major = 0, major + 1
    return parse(f'{major}.{minor}.{patch}-rc.1')[0]


def packed_version(value):
    _, (major, minor, patch) = parse(value)
    return (major << 10) | (minor << 4) | patch


def read_cia_version(path):
    """Read the actual title version from a makerom CIA's signed TMD."""
    with path.open('rb') as file:
        header = file.read(32)
        if len(header) != 32:
            raise ValueError('Truncated CIA header')
        header_size, kind, format_version, certificates, ticket, tmd_size = struct.unpack_from('<IHHIII', header)
        if header_size < 0x2020 or kind != 0 or format_version != 0:
            raise ValueError('Unsupported CIA header')
        # CIA sections align to 64 bytes. The TMD uses an RSA-2048 SHA-256
        # signature block followed by a big-endian title version at body + 0x9c.
        align = lambda size: (size + 63) & ~63
        offset = align(header_size) + align(certificates) + align(ticket)
        if tmd_size < 0x1de or offset + tmd_size > path.stat().st_size:
            raise ValueError('Truncated CIA title metadata')
        file.seek(offset)
        if file.read(4) != b'\x00\x01\x00\x04':
            raise ValueError('Unsupported CIA title metadata signature type')
        file.seek(offset + 0x140 + 0x9c)
        return struct.unpack('>H', file.read(2))[0]
