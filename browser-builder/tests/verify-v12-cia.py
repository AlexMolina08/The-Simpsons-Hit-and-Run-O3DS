#!/usr/bin/env python3
"""Read-only verification of a privately built CIA against v1.2 and PC inputs."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(stream, offset, size):
    stream.seek(offset)
    value = hashlib.sha256()
    while size:
        block = stream.read(min(size, 4 * 1024 * 1024))
        require(block, 'Truncated file during hashing')
        value.update(block)
        size -= len(block)
    return value.hexdigest()


class CIA:
    def __init__(self, path):
        self.path = path
        self.stream = path.open('rb')
        self.size = path.stat().st_size
        header = self.read(0, 0x2020)
        require(struct.unpack_from('<I', header)[0] == 0x2020, 'Invalid CIA header')
        align = lambda n: (n + 63) // 64 * 64
        offset = align(0x2020)
        for field in (8, 12, 16):
            offset = align(offset + struct.unpack_from('<I', header, field)[0])
        self.ncch_offset = offset
        self.ncch = self.read(offset, 0x200)
        require(self.ncch[0x100:0x104] == b'NCCH', 'Missing NCCH')
        require(struct.unpack_from('<Q', self.ncch, 0x118)[0] == 0x000400000f3a1100, 'Wrong Title ID')
        romfs = offset + struct.unpack_from('<I', self.ncch, 0x1b0)[0] * 512
        ivfc = self.read(romfs, 0x60)
        require(ivfc[:4] == b'IVFC', 'Missing RomFS IVFC')
        block = 2 ** struct.unpack_from('<I', ivfc, 0x4c)[0]
        require(block <= 1024 * 1024, 'Invalid IVFC block size')
        level3 = romfs + ((struct.unpack_from('<I', ivfc, 0x54)[0] + struct.unpack_from('<I', ivfc, 8)[0] + block - 1) // block) * block
        info = self.read(level3, 0x28)
        fields = struct.unpack('<10I', info)
        require(fields[9] <= 64 * 1024 * 1024, 'RomFS metadata exceeds test bound')
        metadata = self.read(level3, fields[9])
        directories = metadata[fields[3]:fields[3] + fields[4]]
        files = metadata[fields[7]:fields[7] + fields[8]]
        self.files = {}
        visited = set()

        def visit(directory, prefix):
            require(directory not in visited, 'RomFS directory cycle')
            visited.add(directory)
            item = struct.unpack_from('<I', directories, directory + 12)[0]
            seen = set()
            while item != 0xffffffff:
                require(item not in seen, 'RomFS file cycle')
                seen.add(item)
                parent, sibling, start, length, _, name_size = struct.unpack_from('<IIQQII', files, item)
                name = files[item + 32:item + 32 + name_size].decode('utf-16le')
                require(parent == directory and name and not any(c in name for c in '/\\\0'), 'Invalid RomFS file name')
                key = (prefix + name).lower()
                require(key not in self.files, 'Duplicate case-insensitive RomFS path')
                position = level3 + len(metadata) + start
                require(position + length <= self.size, 'RomFS asset exceeds CIA')
                self.files[key] = (position, length)
                item = sibling
            child = struct.unpack_from('<I', directories, directory + 8)[0]
            siblings = set()
            while child != 0xffffffff:
                require(child not in siblings, 'RomFS sibling cycle')
                siblings.add(child)
                parent, sibling = struct.unpack_from('<II', directories, child)
                name_size = struct.unpack_from('<I', directories, child + 20)[0]
                name = directories[child + 24:child + 24 + name_size].decode('utf-16le')
                require(parent == directory and name and not any(c in name for c in '/\\\0'), 'Invalid RomFS directory')
                visit(child, prefix + name + '/')
                child = sibling
        visit(0, '')

    def read(self, offset, size):
        require(0 <= offset <= offset + size <= self.size, 'Read exceeds CIA')
        self.stream.seek(offset)
        value = self.stream.read(size)
        require(len(value) == size, 'Truncated CIA')
        return value

    def asset(self, path):
        require(path.lower() in self.files, f'Missing {path}')
        return self.files[path.lower()]

    def code_hash(self):
        exefs = self.ncch_offset + struct.unpack_from('<I', self.ncch, 0x1a0)[0] * 512
        header = self.read(exefs, 0x200)
        for index in range(10):
            entry = header[index * 16:index * 16 + 16]
            if entry[:8].rstrip(b'\0') == b'.code':
                offset, size = struct.unpack_from('<II', entry, 8)
                return digest(self.stream, exefs + 0x200 + offset, size)
        raise ValueError('Missing executable code')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cia', required=True, type=Path)
    parser.add_argument('--recipe', required=True, type=Path)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--reference-cia', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    parser.add_argument('--ctrtool', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    recipe = json.loads(args.recipe.read_text())
    report = json.loads(args.report.read_text())
    require(report['version'] == '1.2' and report['sourceCommit'] == recipe['runtime']['sourceCommit'], 'Unexpected build runtime')
    cia, reference = CIA(args.cia), CIA(args.reference_cia)
    require(cia.code_hash() == reference.code_hash(), 'Executable differs from accepted v1.2 package')
    sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/3ds'))
    spec = importlib.util.spec_from_file_location('asset_semantics', Path(__file__).resolve().parents[1] / 'engine-conversion/asset-semantics.py')
    semantics = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(semantics)
    required = {name.lower(): record for name, record in recipe['requiredOverrides'].items() if record.get('required') is not False}
    hud = args.game/'art/frontend/scrooby/ingame.p3d'
    if hud.is_file():
        variant = recipe.get('hudVariants', {}).get(hashlib.sha256(hud.read_bytes()).hexdigest())
        if variant:
            required['art/frontend/scrooby/ingame.p3d'] = variant
    language = report.get('language', 'en')
    archive = {'en': 'dialog.rcf', 'fr': 'dialogf.rcf', 'es': 'dialogs.rcf'}[language]
    offset, size = cia.asset('game-language.txt')
    require(cia.read(offset, size) == (language+'\n').encode(), 'Wrong packaged language')
    exact = semantic = 0
    for name, record in required.items():
        offset, size = cia.asset(name)
        if digest(cia.stream, offset, size) == record['sha256']:
            exact += 1
        else:
            require(record.get('semantic_sha256') and semantics.semantic_fingerprint(cia.read(offset, size)) == record['semantic_sha256'], f'Changed v1.2 enhancement: {name}')
            semantic += 1
    offset, size = cia.asset('native146.bundle')
    require(size == recipe['nativeTextures']['bytes'] and digest(cia.stream, offset, size) == recipe['nativeTextures']['sha256'], 'Native textures differ from accepted release')
    copied = 0
    source_paths = set()
    for path in args.game.rglob('*'):
        if not path.is_file():
            continue
        name = path.relative_to(args.game).as_posix().lower()
        if name in ('dialog.rcf', 'dialogf.rcf', 'dialogs.rcf', 'dialogg.rcf') and name != archive:
            require(name not in cia.files, 'An unused dialogue language was packaged')
            continue
        if path.name.lower() == 'temp.err' or not (name.startswith(('art/', 'scripts/', 'sound/')) or ('/' not in name and name.endswith('.rcf'))):
            continue
        source_paths.add(name)
        if name in required:
            continue
        offset, size = cia.asset(name)
        with path.open('rb') as source:
            require(size == path.stat().st_size and digest(cia.stream, offset, size) == digest(source, 0, size), f'Copied PC data changed: {name}')
        copied += 1
    movies = report['movies']
    for movie in movies:
        name = 'movies/' + movie['file']
        offset, size = cia.asset(name)
        header = cia.read(offset, 52)
        require(header[:8] == b'SRR3DSMV' and struct.unpack_from('<I', header, 8)[0] == 2, f'Invalid movie: {name}')
        frames, audio_bytes, video_offset, video_bytes = [struct.unpack_from('<I', header, index)[0] for index in (24, 40, 44, 48)]
        require((frames, audio_bytes, video_bytes) == (movie['frames'], movie['audioBytes'], movie['videoBytes']) and size == video_offset + video_bytes, f'Movie/report mismatch: {name}')
    require(len(movies) >= 16, 'A required cinematic is missing')
    for index in range(10):
        offset, size = cia.asset(f'sound/3ds-lid/{index:02d}.bin')
        header = cia.read(offset, 12)
        require(header[:4] == b'LID1' and struct.unpack_from('<I', header, 4)[0] == 24000 and size == 12 + struct.unpack_from('<I', header, 8)[0] * 2, 'Invalid character greeting')
    known = source_paths | set(required) | {'native146.bundle', 'game-language.txt'} | {'movies/' + movie['file'].lower() for movie in movies} | {f'sound/3ds-lid/{i:02d}.bin' for i in range(10)}
    require(set(cia.files) <= known, f'Unexpected packaged paths: {set(cia.files) - known}')
    verification = subprocess.run([str(args.ctrtool), '--intype=cia', '--verify', str(args.cia)], text=True, capture_output=True, check=True)
    log = verification.stdout + verification.stderr
    require('1.2.0 (1056)' in log and not any(('FAIL' in line or 'ERROR' in line) and 'Signature' not in line for line in log.splitlines()), 'Container verification failed')
    result = {'passed': True, 'ciaBytes': cia.size, 'ciaSHA256': digest(cia.stream, 0, cia.size), 'romfsFiles': len(cia.files), 'requiredExact': exact, 'requiredLosslessPNGEquivalent': semantic, 'copiedPCFilesExact': copied, 'movies': len(movies), 'nativeTextureEntries': recipe['nativeTextures']['entries'], 'nativeTexturesExact': True, 'acceptedExecutableExact': True, 'titleID': recipe['titleID'], 'packedVersion': 1056, 'buildTimings': report['timings'], 'scope': 'Desktop browser artifact and private PC/accepted-asset parity; no console or mobile claim'}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    args.out.with_suffix('.ctrtool.log').write_text(log)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
