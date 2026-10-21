#!/usr/bin/env python3
"""Prepare a short HOME Menu cue from the user's retail PC music archive."""
import argparse
from array import array
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import wave

from setup_sd import find_ffmpeg

TRACK = r'sound\music\simpsons_theme.rsd'
DURATION = 2.8
RATE = 32000


def archive_member(archive, name):
    """Read one retail RCF member using radCFHeader and its case-insensitive key."""
    key = 0
    for char in name:
        value = ord(char)
        value = value + 32 if value < ord('a') else value
        key = (31 * key + value) & 0xffffffff
    with archive.open('rb') as source:
        size = source.seek(0, 2)
        source.seek(0)
        info = source.read(48)
        if len(info) != 48 or not info.startswith(b'RADCORE CEMENT LIBRARY') or info[34] != 0:
            raise ValueError(f'Not a little-endian retail RCF: {archive}')
        header, = struct.unpack_from('<I', info, 44)
        if header + 16 > size:
            raise ValueError('RCF header is outside the archive')
        source.seek(header)
        count, _, _, _ = struct.unpack('<4I', source.read(16))
        if count > 100000 or header + 16 + count * 12 > size:
            raise ValueError('Invalid RCF directory')
        for _ in range(count):
            hashed, offset, length = struct.unpack('<3I', source.read(12))
            if hashed == key:
                if not length or offset + length > size:
                    raise ValueError('RCF member is outside the archive')
                source.seek(offset)
                return source.read(length)
    raise ValueError(f'Missing {name} in {archive.name}')


def prepare(data, output, ffmpeg=None):
    source = archive_member(data / 'music02.rcf', TRACK)
    output.mkdir(parents=True, exist_ok=True)
    cue = output / 'banner.wav'
    with tempfile.TemporaryDirectory(prefix='srr-banner-') as temporary:
        track = Path(temporary) / 'theme.rsd'
        track.write_bytes(source)
        subprocess.run([find_ffmpeg(ffmpeg), '-hide_banner', '-loglevel', 'error', '-y',
                        '-i', str(track), '-t', str(DURATION), '-af',
                        'loudnorm=I=-20:TP=-2:LRA=7,afade=t=in:d=0.015,afade=t=out:st=2.6:d=0.2',
                        '-ar', str(RATE), '-ac', '2', '-c:a', 'pcm_s16le', str(cue)], check=True)
    with wave.open(str(cue)) as audio:
        if (audio.getframerate(), audio.getnchannels(), audio.getsampwidth(), audio.getnframes()) != (RATE, 2, 2, int(RATE * DURATION)):
            raise ValueError('Unexpected HOME Menu audio format')
        samples = array('h', audio.readframes(audio.getnframes()))
        peak = max(abs(sample) for sample in samples)
        if not 32 < peak < 26900:
            raise ValueError('HOME Menu cue is silent or exceeds the peak allowance')
    (output / 'audio-source.json').write_text(json.dumps({
        'archive': 'music02.rcf', 'member': TRACK,
        'source_sha256': hashlib.sha256(source).hexdigest(),
        'seconds': DURATION, 'sample_rate': RATE, 'channels': 2, 'bits': 16,
        'loop': False, 'peak_pcm16': peak, 'cue_sha256': hashlib.sha256(cue.read_bytes()).hexdigest(),
    }, indent=2) + '\n')
    return cue


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=Path('sdcard/3ds/simpsons'))
    parser.add_argument('--out', type=Path, default=Path('out/home-menu'))
    parser.add_argument('--ffmpeg')
    args = parser.parse_args()
    print(prepare(args.data, args.out, args.ffmpeg))
