"""Version handling shared by CIA packaging and the local release launcher."""
import re

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
